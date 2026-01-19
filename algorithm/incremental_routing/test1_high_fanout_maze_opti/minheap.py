class MinHeap:
    def __init__(self):
        self.heap = []  # 存放 (cost, (x, y))
        self.pos_map = {}  # (x, y) -> index in heap

    def _swap(self, i, j):
        self.heap[i], self.heap[j] = self.heap[j], self.heap[i]
        self.pos_map[self.heap[i][1]] = i
        self.pos_map[self.heap[j][1]] = j

    def _sift_up(self, idx):
        while idx > 0:
            parent = (idx - 1) // 2
            if self.heap[idx][0] < self.heap[parent][0]:
                self._swap(idx, parent)
                idx = parent
            else:
                break

    def _sift_down(self, idx):
        n = len(self.heap)
        while True:
            left = 2 * idx + 1
            right = 2 * idx + 2
            smallest = idx

            if left < n and self.heap[left][0] < self.heap[smallest][0]:
                smallest = left
            if right < n and self.heap[right][0] < self.heap[smallest][0]:
                smallest = right
            if smallest != idx:
                self._swap(idx, smallest)
                idx = smallest
            else:
                break

    def heapify(self, arr):
        self.heap = arr[:]
        self.pos_map.clear()
        for i, item in enumerate(self.heap):
            if not isinstance(item, tuple) or not isinstance(item[1], tuple):
                raise ValueError(f"Invalid heap item: {item} with index: {i}, expected (cost, (x, y))")
            self.pos_map[item[1]] = i
        for i in reversed(range(len(self.heap) // 2)):
            self._sift_down(i)

    def push(self, item):  # item: (cost, (x, y))
        self.heap.append(item)
        idx = len(self.heap) - 1
        self.pos_map[item[1]] = idx
        self._sift_up(idx)

    def pop(self):
        if not self.heap:
            return None
        self._swap(0, len(self.heap) - 1)
        item = self.heap.pop()
        del self.pos_map[item[1]]
        if self.heap:
            self._sift_down(0)
        return item

    def remove(self, coord):  # coord: (x, y)
        idx = self.pos_map.get(coord)
        if idx is None:
            return False  # 不在堆里
        self._swap(idx, len(self.heap) - 1)
        removed = self.heap.pop()
        del self.pos_map[coord]
        if idx < len(self.heap):
            self._sift_up(idx)
            self._sift_down(idx)
        return True

    def has(self, coord):
        idx = self.pos_map.get(coord)
        if idx:
            return True
        else:
            return False

    def __len__(self):
        return len(self.heap)

    def __contains__(self, coord):
        return coord in self.pos_map

    def __str__(self):
        return str(self.heap)
