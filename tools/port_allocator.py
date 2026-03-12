#!/usr/bin/env python3
"""Automatic connection allocator for Kiwi test constraints.

Given a constraint directory containing:
1. 01_ports.json
2. external_ports.json
3. topdie_insts.json
4. topdies.json (or topdie.json)
5. connection.txt (default name, configurable)

This script allocates chip ports and outputs connections.json in the same directory.

About connection.txt format:
- Lines starting with '#' are comments. Supported inline comment tokens are '\\', '\', and '//'.
- Mode markers can be specified as '# mode 1' or '#mode 2' to separate requests into different modes. If no explicit mode markers are present, the mode info will be removed.
- A special section for usable external ports can be defined with '# usable_ex_port', followed by lines listing external port names. Only these ports will be considered for chip-to-board connections, and they must exist in the pin_map of at least one topdie.
- Request lines can be in one of the following formats:
  - For pose/nege connections: '<chip> pose <count>' or '<chip> nege <count>'
  - For simple/bus connections: '<src> <dst> simple <count>' or '<src> <dst> bus <count>'
    where <src> and <dst> can be chip instance names or the board name (currently "xinzhai"). 'simple' means each connection is independent, while 'bus' means a high-speed bus net, and all connections with the same src/dst/count form a group that must be allocated identically across modes if repeated.
- A special section for multi-fanout chip ports can be defined with '# multi_fanout_chip_port', followed by lines listing chip port names. These ports are allowed to be reused multiple times within the same mode.
"""

from __future__ import annotations

import argparse
import json
import re
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Set, Tuple


class AllocationError(RuntimeError):
	"""Raised when constraints cannot be satisfied."""


@dataclass
class TopDieInst:
	name: str
	topdie_type: str


@dataclass
class Request:
	kind: str  # bus/simple/pose/nege
	src: str
	dst: str
	count: int


def _check_file_exists_and_nonempty(file_path: Path) -> None:
	"""Validate that the given path exists, is a file, and is not empty."""
	if not file_path.exists():
		raise AllocationError(f"Missing required file: {file_path}")
	if not file_path.is_file():
		raise AllocationError(f"Path is not a file: {file_path}")
	if file_path.stat().st_size == 0:
		raise AllocationError(f"Required file is empty: {file_path}")


def _load_json(file_path: Path):
	"""Load and parse JSON file content with a clear error on decode failure."""
	try:
		return json.loads(file_path.read_text(encoding="utf-8"))
	except json.JSONDecodeError as exc:
		raise AllocationError(f"Invalid JSON in {file_path}: {exc}") from exc


def _strip_inline_comment(line: str) -> str:
	"""Remove supported inline comments and return trimmed content."""
	# Support '\\', '\', and '//' comments anywhere in line.
	for token in ("\\\\", "//", "\\"):
		idx = line.find(token)
		if idx >= 0:
			line = line[:idx]
	return line.strip()


def _parse_mode_marker(line: str) -> str | None:
	"""Parse a mode marker and return '1'/'2' when matched."""
	normalized = line.strip().lower()
	if normalized.startswith("#"):
		normalized = normalized[1:].strip()
	match = re.fullmatch(r"mode\s+([12])", normalized)
	if match:
		return match.group(1)
	return None


def _parse_connection_txt(
	path: Path, valid_topdie_ports: Set[str]
) -> Tuple[Dict[str, List[Request]], List[str], List[str], bool]:
	"""Parse connection.txt into requests and special chip-port policy lists."""
	raw_lines = path.read_text(encoding="utf-8").splitlines()

	requests_by_mode: Dict[str, List[Request]] = {"1": []}
	seen_request_keys_by_mode: Dict[str, Set[Tuple[str, str, str, int]]] = {"1": set()}
	current_mode = "1"
	has_explicit_modes = False
	in_usable_section = False
	in_multi_fanout_section = False
	usable_ex_ports: List[str] = []
	multi_fanout_chip_ports: List[str] = []

	for line_no, raw in enumerate(raw_lines, start=1):
		stripped = raw.strip()
		if not stripped:
			continue

		if stripped.startswith("#"):
			comment_body = _strip_inline_comment(stripped[1:].strip())
			marker = comment_body.lower()
			if marker.startswith("usable_ex_port"):
				in_usable_section = True
				in_multi_fanout_section = False
				continue
			if marker.startswith("multi_fanout_chip_port"):
				in_multi_fanout_section = True
				in_usable_section = False
				continue
			mode = _parse_mode_marker(comment_body)
			if mode is not None:
				has_explicit_modes = True
				current_mode = mode
				requests_by_mode.setdefault(current_mode, [])
				seen_request_keys_by_mode.setdefault(current_mode, set())
				in_usable_section = False
				in_multi_fanout_section = False
			continue

		if stripped.startswith("\\"):
			continue

		line = _strip_inline_comment(raw)
		if not line:
			continue

		mode = _parse_mode_marker(line)
		if mode is not None:
			has_explicit_modes = True
			current_mode = mode
			requests_by_mode.setdefault(current_mode, [])
			seen_request_keys_by_mode.setdefault(current_mode, set())
			in_usable_section = False
			in_multi_fanout_section = False
			continue

		if line.lower() == "usable_ex_port":
			in_usable_section = True
			in_multi_fanout_section = False
			continue

		if line.lower() == "multi_fanout_chip_port":
			in_multi_fanout_section = True
			in_usable_section = False
			continue

		if in_usable_section:
			port_name = line.strip()
			if port_name not in valid_topdie_ports:
				raise AllocationError(
					f"Line {line_no}: usable_ex_port '{port_name}' "
					"not found in topdies pin_map"
				)
			usable_ex_ports.append(port_name)
			continue

		if in_multi_fanout_section:
			port_name = line.strip()
			if port_name not in valid_topdie_ports:
				raise AllocationError(
					f"Line {line_no}: multi_fanout_chip_port '{port_name}' "
					"not found in topdies pin_map"
				)
			multi_fanout_chip_ports.append(port_name)
			continue

		parts = line.split()
		if len(parts) == 3 and parts[1].lower() in {"pose", "nege"}:
			src = parts[0]
			dst = parts[1].lower()
			count = _parse_positive_int(parts[2], line_no)
			req_key = (dst, src, dst, count)
			if req_key in seen_request_keys_by_mode[current_mode]:
				raise AllocationError(
					f"Line {line_no}: duplicated interconnection description in mode {current_mode}: '{line}'"
				)
			seen_request_keys_by_mode[current_mode].add(req_key)
			requests_by_mode[current_mode].append(
				Request(kind=dst, src=src, dst=dst, count=count)
			)
			continue

		if len(parts) == 4 and parts[2].lower() in {"bus", "simple"}:
			src, dst, kind, count_raw = parts
			kind = kind.lower()
			count = _parse_positive_int(count_raw, line_no)
			req_key = (kind, src, dst, count)
			if req_key in seen_request_keys_by_mode[current_mode]:
				raise AllocationError(
					f"Line {line_no}: duplicated interconnection description in mode {current_mode}: '{line}'"
				)
			seen_request_keys_by_mode[current_mode].add(req_key)
			requests_by_mode[current_mode].append(
				Request(kind=kind, src=src, dst=dst, count=count)
			)
			continue

		raise AllocationError(f"Line {line_no}: unrecognized format: '{raw}'")

	return requests_by_mode, usable_ex_ports, multi_fanout_chip_ports, has_explicit_modes


def _parse_positive_int(value: str, line_no: int) -> int:
	"""Parse a strictly positive integer for one request field."""
	try:
		number = int(value)
	except ValueError as exc:
		raise AllocationError(f"Line {line_no}: '{value}' is not an integer") from exc
	if number <= 0:
		raise AllocationError(f"Line {line_no}: value must be > 0, got {number}")
	return number


def _build_chip_port_index(
	topdie_insts: Dict, topdies: Dict, usable_ex_ports: Set[str]
) -> Tuple[Dict[str, List[str]], Dict[str, List[str]], Dict[str, Dict[str, int]]]:
	"""Build sorted per-chip port lists for all/intra categories."""
	inst_port_any: Dict[str, List[str]] = {}
	inst_port_intra: Dict[str, List[str]] = {}
	inst_pin_map: Dict[str, Dict[str, int]] = {}

	for inst_name, inst_data in topdie_insts.items():
		if "topdie" not in inst_data:
			raise AllocationError(
				f"topdie_insts entry '{inst_name}' missing required field 'topdie'"
			)
		topdie_type = inst_data["topdie"]
		if topdie_type not in topdies:
			raise AllocationError(
				f"topdie_insts entry '{inst_name}' references unknown topdie '{topdie_type}'"
			)
		pin_map = topdies[topdie_type].get("pin_map")
		if not isinstance(pin_map, dict) or not pin_map:
			raise AllocationError(
				f"topdies '{topdie_type}' has missing/empty pin_map definition"
			)

		sorted_ports = sorted(pin_map.keys(), key=lambda p: int(pin_map[p]))
		inst_port_any[inst_name] = list(sorted_ports)
		inst_port_intra[inst_name] = [p for p in sorted_ports if p not in usable_ex_ports]
		inst_pin_map[inst_name] = {k: int(v) for k, v in pin_map.items()}

	return inst_port_any, inst_port_intra, inst_pin_map


def _parse_board_name(external_ports: Dict[str, Dict]) -> str:
	"""Infer the board name prefix from external port naming."""
	if not external_ports:
		raise AllocationError("external_ports.json is empty")
	sample = next(iter(external_ports.keys()))
	if "_" not in sample:
		raise AllocationError(
			"external port naming should be '<board>_<port>', but got: " + sample
		)
	return sample.split("_", 1)[0]


class Allocator:
	def __init__(
		self,
		topdie_insts: Dict,
		topdies: Dict,
		external_ports: Dict,
		one_ports: Dict,
		usable_ex_ports: List[str],
		multi_fanout_chip_ports: List[str],
	) -> None:
		"""Initialize allocation state, capacities, and per-chip port pools."""
		self.topdie_insts = {
			name: TopDieInst(name=name, topdie_type=data["topdie"])
			for name, data in topdie_insts.items()
		}
		self.chips: Set[str] = set(self.topdie_insts.keys())
		self.external_ports = external_ports
		self.available_external: Set[str] = set(external_ports.keys())
		self.used_external: Set[str] = set()

		self.board_name = _parse_board_name(external_ports)

		self.pose_capacity = len(one_ports.get("pose", {}))
		self.nege_capacity = len(one_ports.get("nege", {}))
		if self.pose_capacity == 0 or self.nege_capacity == 0:
			raise AllocationError("01_ports.json must contain non-empty 'pose' and 'nege'")
		self.pose_used = 0
		self.nege_used = 0

		self.usable_ex_ports = set(usable_ex_ports)
		all_topdie_ports = {
			p for t in topdies.values() for p in (t.get("pin_map") or {}).keys()
		}
		if self.usable_ex_ports - all_topdie_ports:
			missing = sorted(self.usable_ex_ports - all_topdie_ports)
			raise AllocationError(
				"usable_ex_port entries not found in topdies pin_map: " + ", ".join(missing)
			)

		self.multi_fanout_chip_ports = set(multi_fanout_chip_ports)
		if self.multi_fanout_chip_ports - all_topdie_ports:
			missing = sorted(self.multi_fanout_chip_ports - all_topdie_ports)
			raise AllocationError(
				"multi_fanout_chip_port entries not found in topdies pin_map: "
				+ ", ".join(missing)
			)

		self.inst_port_any, self.inst_port_intra, self.inst_pin_map = _build_chip_port_index(
			topdie_insts, topdies, self.usable_ex_ports
		)
		self.used_chip_ports: Dict[str, Set[str]] = {c: set() for c in self.chips}

	def _validate_chip(self, chip: str) -> None:
		"""Ensure a chip instance name exists in topdie_insts."""
		if chip not in self.chips:
			raise AllocationError(f"Unknown chip instance '{chip}'")

	def _reset_mode_usage(self) -> None:
		"""Reset all per-mode usage so different modes allocate independently."""
		# External board IO ports are reusable within a mode (one-to-many).
		self.used_external = set()
		self.used_chip_ports = {c: set() for c in self.chips}
		self.pose_used = 0
		self.nege_used = 0

	def _consume_existing_pair_usage(self, pair: List[str]) -> None:
		"""Mark resources in an existing pair as used in current mode state."""
		if len(pair) != 2:
			raise AllocationError(f"Invalid connection pair format: {pair}")

		for endpoint in pair:
			if endpoint in (f"{self.board_name}_pose", f"{self.board_name}_nege"):
				if endpoint.endswith("_pose"):
					self.pose_used += 1
					if self.pose_used > self.pose_capacity:
						raise AllocationError(
							"pose usage exceeds 01_ports capacity while replaying mode data"
						)
				else:
					self.nege_used += 1
					if self.nege_used > self.nege_capacity:
						raise AllocationError(
							"nege usage exceeds 01_ports capacity while replaying mode data"
						)
				continue

			if endpoint in self.external_ports:
				# External board IO is allowed to fan out to multiple chip ports.
				self.used_external.add(endpoint)
				continue

			if "." in endpoint:
				chip, chip_port = endpoint.split(".", 1)
				self._validate_chip(chip)
				if (
					chip_port in self.used_chip_ports[chip]
					and chip_port not in self.multi_fanout_chip_ports
				):
					raise AllocationError(
						f"Duplicate chip port '{endpoint}' inside one mode"
					)
				if chip_port not in self.multi_fanout_chip_ports:
					self.used_chip_ports[chip].add(chip_port)
				continue

			raise AllocationError(f"Unknown endpoint format in pair: {endpoint}")

	def _pick_chip_port(self, chip: str, category: str) -> str:
		"""Allocate one unused chip port according to requested category."""
		self._validate_chip(chip)

		if category == "external":
			candidates = self.inst_port_any[chip]
			allowed = self.usable_ex_ports
		elif category == "intra":
			candidates = self.inst_port_intra[chip]
			allowed = None
		elif category == "any":
			candidates = self.inst_port_any[chip]
			allowed = None
		else:
			raise AllocationError(f"Internal error: unknown port category '{category}'")

		for port in candidates:
			if allowed is not None and port not in allowed:
				continue
			if not self._chip_port_available(chip, port):
				continue
			self._mark_chip_port_used(chip, port)
			return port

		if category == "external":
			raise AllocationError(
				f"No available chip port for external IO on '{chip}'. "
				f"Require one of usable_ex_port set; already used all candidates."
			)
		if category == "intra":
			raise AllocationError(
				f"No available chip-to-chip port left on '{chip}' "
				"(usable_ex_port ports are excluded)."
			)
		raise AllocationError(f"No available chip port left on '{chip}'")

	def _chip_port_available(self, chip: str, port: str) -> bool:
		"""Check if a chip port can still be used in current mode."""
		if port in self.multi_fanout_chip_ports:
			return True
		return port not in self.used_chip_ports[chip]

	def _mark_chip_port_used(self, chip: str, port: str) -> None:
		"""Mark chip port as used unless it is configured for multi-fanout."""
		if port in self.multi_fanout_chip_ports:
			return
		self.used_chip_ports[chip].add(port)

	def _pick_shared_chip_port(self, left_chip: str, right_chip: str, category: str) -> str:
		"""Pick one shared port name that is available on both chips."""
		self._validate_chip(left_chip)
		self._validate_chip(right_chip)

		if category == "intra":
			left_candidates = self.inst_port_intra[left_chip]
			right_candidates = self.inst_port_intra[right_chip]
		elif category == "any":
			left_candidates = self.inst_port_any[left_chip]
			right_candidates = self.inst_port_any[right_chip]
		else:
			raise AllocationError(f"Internal error: unknown shared port category '{category}'")

		right_set = set(right_candidates)
		for port in left_candidates:
			if port not in right_set:
				continue
			if not self._chip_port_available(left_chip, port):
				continue
			if not self._chip_port_available(right_chip, port):
				continue
			self._mark_chip_port_used(left_chip, port)
			self._mark_chip_port_used(right_chip, port)
			return port

		raise AllocationError(
			f"No shared available chip port name for '{left_chip}' and '{right_chip}' "
			"under same-name chip-to-chip rule."
		)

	def _external_suffix(self, external_name: str) -> str:
		"""Extract external port suffix after the board-name prefix."""
		prefix = f"{self.board_name}_"
		if not external_name.startswith(prefix):
			raise AllocationError(
				f"External port '{external_name}' does not start with board prefix '{prefix}'"
			)
		return external_name[len(prefix) :]

	def _external_compatible(self, chip: str, chip_port: str, external_name: str) -> bool:
		"""Check naming-rule compatibility between chip port and external port."""
		suffix = self._external_suffix(external_name)

		# If external name is chip-specific (e.g. muyan_0_interrupt_0), enforce same chip.
		for chip_name in self.chips:
			token = f"{chip_name}_"
			if suffix.startswith(token) and chip_name != chip:
				return False

		lower_suffix = suffix.lower()
		lower_chip_port = chip_port.lower()
		if "din" in lower_suffix:
			return "dout" in lower_chip_port
		if "dout" in lower_suffix:
			return "din" in lower_chip_port
		return suffix == chip_port or suffix.endswith("_" + chip_port)

	def _pick_external_port(self, chip: str, chip_port: str) -> str:
		"""Allocate one compatible external IO port for a chip port."""
		sorted_candidates = sorted(self.available_external)
		for external_name in sorted_candidates:
			if self._external_compatible(chip, chip_port, external_name):
				return external_name

		raise AllocationError(
			f"No external IO port available for chip port '{chip}.{chip_port}' "
			"with required naming rules."
		)

	def _alloc_chip_chip_pair(self, left_chip: str, right_chip: str) -> List[str]:
		"""Allocate one endpoint pair for a chip-to-chip connection."""
		self._validate_chip(left_chip)
		self._validate_chip(right_chip)

		if left_chip == right_chip:
			port = self._pick_chip_port(left_chip, category="intra")
			return [f"{left_chip}.{port}", f"{right_chip}.{port}"]

		shared_port = self._pick_shared_chip_port(left_chip, right_chip, category="intra")
		return [f"{left_chip}.{shared_port}", f"{right_chip}.{shared_port}"]

	def _alloc_chip_external_pair(self, chip: str, board_side_name: str) -> List[str]:
		"""Allocate one endpoint pair for a chip-to-board-IO connection."""
		self._validate_chip(chip)
		if board_side_name != self.board_name:
			raise AllocationError(
				f"Unknown board endpoint '{board_side_name}', expected '{self.board_name}'"
			)

		chip_port = self._pick_chip_port(chip, category="external")
		external_port = self._pick_external_port(chip, chip_port)
		return [f"{chip}.{chip_port}", external_port]

	def _alloc_chip_pose_nege_pair(self, chip: str, kind: str) -> List[str]:
		"""Allocate one endpoint pair for chip to pose/nege board port."""
		self._validate_chip(chip)
		if kind not in {"pose", "nege"}:
			raise AllocationError(f"Internal error: unknown kind '{kind}'")

		if kind == "pose":
			if self.pose_used >= self.pose_capacity:
				raise AllocationError(
					"No available pose port left in 01_ports.json for requested allocations"
				)
			self.pose_used += 1
		else:
			if self.nege_used >= self.nege_capacity:
				raise AllocationError(
					"No available nege port left in 01_ports.json for requested allocations"
				)
			self.nege_used += 1

		chip_port = self._pick_chip_port(chip, category="any")
		return [f"{self.board_name}_{kind}", f"{chip}.{chip_port}"]

	def allocate_requests(
		self, requests_by_mode: Dict[str, List[Request]], has_explicit_modes: bool
	) -> Dict:
		"""Allocate all requested links and build final connections JSON object."""
		ordered_modes = sorted(requests_by_mode.keys(), key=lambda x: int(x))
		connections_by_mode: Dict[str, Dict[str, List[List[str]]]] = {}

		# Global bus-group numbering across modes is keyed by semantic signature:
		# (unordered endpoints, bitwidth). Only matched signatures share group IDs.
		signature_to_group_id: Dict[Tuple[str, str, int], int] = {}
		signature_to_pairs: Dict[Tuple[str, str, int], List[List[str]]] = {}
		next_global_bus_group = 1

		# Simple reuse across modes is keyed by directional signature and occurrence index.
		simple_signature_to_occurrence_pairs: Dict[
			Tuple[str, str, int], List[List[List[str]]]
		] = defaultdict(list)

		for mode in ordered_modes:
			self._reset_mode_usage()
			mode_groups: Dict[str, List[List[str]]] = {"-1": []}
			simple_signature_occurrence_seen: Dict[Tuple[str, str, int], int] = defaultdict(int)

			for req in requests_by_mode[mode]:
				if req.kind in {"pose", "nege"}:
					for _ in range(req.count):
						pair = self._alloc_chip_pose_nege_pair(req.src, req.kind)
						mode_groups["-1"].append(pair)
					continue

				if req.kind == "simple":
					signature = self._simple_signature(req)
					occ_idx = simple_signature_occurrence_seen[signature]
					simple_signature_occurrence_seen[signature] += 1

					if occ_idx < len(simple_signature_to_occurrence_pairs[signature]):
						pairs = simple_signature_to_occurrence_pairs[signature][occ_idx]
						for pair in pairs:
							mode_groups["-1"].append(list(pair))
							self._consume_existing_pair_usage(pair)
						continue

					pairs: List[List[str]] = []
					for _ in range(req.count):
						pair = self._alloc_pair_by_endpoints(req.src, req.dst)
						pairs.append(pair)
						mode_groups["-1"].append(pair)
					simple_signature_to_occurrence_pairs[signature].append(
						[list(p) for p in pairs]
					)
					continue

				if req.kind == "bus":
					signature = self._bus_signature(req)

					if signature in signature_to_group_id:
						group_id = str(signature_to_group_id[signature])
						if group_id in mode_groups:
							raise AllocationError(
								f"Mode {mode} has duplicated bus definition for group '{group_id}'"
							)
						bus_pairs = signature_to_pairs[signature]
						mode_groups[group_id] = [list(p) for p in bus_pairs]
						for pair in bus_pairs:
							self._consume_existing_pair_usage(pair)
						continue

					group_id = str(next_global_bus_group)
					signature_to_group_id[signature] = next_global_bus_group
					next_global_bus_group += 1

					bus_pairs: List[List[str]] = []
					for _ in range(req.count):
						pair = self._alloc_pair_by_endpoints(req.src, req.dst)
						bus_pairs.append(pair)
					mode_groups[group_id] = bus_pairs
					signature_to_pairs[signature] = [list(p) for p in bus_pairs]
					continue

				raise AllocationError(f"Internal error: unsupported request kind '{req.kind}'")

			connections_by_mode[mode] = mode_groups

		self._validate_cross_mode_bus_groups(connections_by_mode)

		if has_explicit_modes:
			return connections_by_mode

		return connections_by_mode.get("1", {"-1": []})

	@staticmethod
	def _bus_signature(req: Request) -> Tuple[str, str, int]:
		"""Build canonical bus signature (unordered endpoints + bitwidth)."""
		left, right = sorted([req.src, req.dst])
		return (left, right, req.count)

	@staticmethod
	def _simple_signature(req: Request) -> Tuple[str, str, int]:
		"""Build directional simple signature (src, dst, number)."""
		return (req.src, req.dst, req.count)

	def _alloc_pair_by_endpoints(self, src: str, dst: str) -> List[str]:
		"""Dispatch endpoint allocation based on chip/board endpoint types."""
		src_is_chip = src in self.chips
		dst_is_chip = dst in self.chips
		src_is_board = src == self.board_name
		dst_is_board = dst == self.board_name

		if src_is_chip and dst_is_chip:
			return self._alloc_chip_chip_pair(src, dst)
		if src_is_chip and dst_is_board:
			return self._alloc_chip_external_pair(src, dst)
		if src_is_board and dst_is_chip:
			pair = self._alloc_chip_external_pair(dst, src)
			return [pair[1], pair[0]]

		raise AllocationError(
			f"Unsupported endpoints '{src}' -> '{dst}'. "
			f"Expected chip-chip or chip-{self.board_name}."
		)

	def _pair_matches_request(self, pair: List[str], req: Request) -> bool:
		"""Check whether an allocated pair matches a request's endpoint pattern."""
		if len(pair) != 2:
			return False

		left, right = pair

		def _endpoint_matches(endpoint: str, target: str) -> bool:
			"""Check if one endpoint string matches one logical request target."""
			if target == self.board_name:
				return endpoint in self.external_ports
			if target in self.chips:
				return endpoint.startswith(target + ".")
			return False

		return _endpoint_matches(left, req.src) and _endpoint_matches(right, req.dst)

	@staticmethod
	def _validate_cross_mode_bus_groups(connections_by_mode: Dict[str, Dict[str, List[List[str]]]]) -> None:
		"""Ensure same positive bus group IDs are identical across modes."""
		bus_group_ids: Set[str] = set()
		for mode_groups in connections_by_mode.values():
			for gid in mode_groups.keys():
				if gid != "-1":
					bus_group_ids.add(gid)

		for gid in sorted(bus_group_ids, key=lambda x: int(x)):
			baseline = None
			baseline_mode = None
			for mode, groups in sorted(connections_by_mode.items(), key=lambda x: int(x[0])):
				if gid not in groups:
					continue
				if baseline is None:
					baseline = groups[gid]
					baseline_mode = mode
				elif groups[gid] != baseline:
					raise AllocationError(
						f"Bus group '{gid}' differs between mode {baseline_mode} and mode {mode}, "
						"but same positive group ID across modes must be identical."
					)


def _collect_required_files(constraint_dir: Path, connection_filename: str) -> Dict[str, Path]:
	"""Resolve and validate all required input files for one run."""
	topdies = constraint_dir / "topdies.json"
	topdie_single = constraint_dir / "topdie.json"
	if topdies.exists():
		topdie_file = topdies
	elif topdie_single.exists():
		topdie_file = topdie_single
	else:
		topdie_file = topdies

	files = {
		"01_ports": constraint_dir / "01_ports.json",
		"external_ports": constraint_dir / "external_ports.json",
		"topdie_insts": constraint_dir / "topdie_insts.json",
		"topdies": topdie_file,
		"connection_txt": constraint_dir / connection_filename,
	}
	for fp in files.values():
		_check_file_exists_and_nonempty(fp)
	return files


def main() -> int:
	"""Parse CLI args, run allocation, and write the output JSON file."""
	parser = argparse.ArgumentParser(
		description="Allocate ports and generate connections.json from connection.txt"
	)
	parser.add_argument(
		"constraint_dir",
		help="Constraint directory containing 01_ports.json, external_ports.json, topdie_insts.json, topdies.json/topdie.json and connection.txt",
	)
	parser.add_argument(
		"--connection-file",
		default="connection.txt",
		help="Connection request file name inside constraint_dir (default: connection.txt)",
	)
	parser.add_argument(
		"--output",
		default="connections.json",
		help="Output file name inside constraint_dir (default: connections.json)",
	)

	args = parser.parse_args()
	constraint_dir = Path(args.constraint_dir).resolve()

	if not constraint_dir.exists() or not constraint_dir.is_dir():
		raise AllocationError(f"Invalid constraint directory: {constraint_dir}")

	files = _collect_required_files(constraint_dir, args.connection_file)

	one_ports = _load_json(files["01_ports"])
	external_ports = _load_json(files["external_ports"])
	topdie_insts = _load_json(files["topdie_insts"])
	topdies = _load_json(files["topdies"])

	if not isinstance(topdie_insts, dict) or not topdie_insts:
		raise AllocationError("topdie_insts.json must be a non-empty object")
	if not isinstance(topdies, dict) or not topdies:
		raise AllocationError("topdies/topdie JSON must be a non-empty object")
	if not isinstance(external_ports, dict) or not external_ports:
		raise AllocationError("external_ports.json must be a non-empty object")
	if not isinstance(one_ports, dict) or not one_ports:
		raise AllocationError("01_ports.json must be a non-empty object")

	valid_topdie_ports: Set[str] = set()
	for topdie_name, topdie_data in topdies.items():
		if "pin_map" not in topdie_data or not isinstance(topdie_data["pin_map"], dict):
			raise AllocationError(f"topdie '{topdie_name}' missing valid pin_map")
		valid_topdie_ports.update(topdie_data["pin_map"].keys())

	requests_by_mode, usable_ex_ports, multi_fanout_chip_ports, has_explicit_modes = _parse_connection_txt(
		files["connection_txt"], valid_topdie_ports
	)
	allocator = Allocator(
		topdie_insts=topdie_insts,
		topdies=topdies,
		external_ports=external_ports,
		one_ports=one_ports,
		usable_ex_ports=usable_ex_ports,
		multi_fanout_chip_ports=multi_fanout_chip_ports,
	)

	connections_obj = allocator.allocate_requests(
		requests_by_mode=requests_by_mode,
		has_explicit_modes=has_explicit_modes,
	)

	output_path = constraint_dir / args.output
	output_path.write_text(
		json.dumps(connections_obj, ensure_ascii=False, indent=4) + "\n",
		encoding="utf-8",
	)
	print(f"Generated: {output_path}")
	return 0


if __name__ == "__main__":
	try:
		raise SystemExit(main())
	except AllocationError as exc:
		print(f"ERROR: {exc}")
		raise SystemExit(1)
