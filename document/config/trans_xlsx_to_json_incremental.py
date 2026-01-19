import pandas as pd
import json
import re

def parse_excel_simple(file_path):
    mode_col, input_col, output_col, tag_col = 5, 6, 7, 8
    
    try:
        df = pd.read_excel(file_path, header=None)
        
        # 获取mode值
        mode_value = df.iloc[1, mode_col]
        
        # 收集数据行
        groups = {}
        for i in range(2, len(df)):
            row = df.iloc[i]
            
            # 跳过空行
            if pd.isna(row[input_col]) or pd.isna(row[output_col]):
                continue
            
            # 处理input和output格式（使用相同的转换方法）
            def convert_value(val):
                val_str = str(val)
                if val_str.startswith("muyan_"):
                    return re.sub(r'muyan_(\d+)_(.+)', r'muyan_\1.\2', val_str)
                return val_str
            
            input_val = convert_value(row[input_col])
            output_val = convert_value(row[output_col])
            tag = str(row[tag_col]) if not pd.isna(row[tag_col]) else "default"
            
            if tag not in groups:
                groups[tag] = []
            groups[tag].append([input_val, output_val])
        
        result = {str(mode_value): groups}
        return json.dumps(result, indent=2, ensure_ascii=False)
    
    except Exception as e:
        print(f"错误: {e}")
        return None

# 使用示例
if __name__ == "__main__":
    result = parse_excel_simple("./document/config/ring_case.xlsx")
    if result:
        try:
            with open("./document/config/ring_case.json", 'w', encoding='utf-8') as json_file:
                json_file.write(result)
        except Exception as e:
            print(f"错误: {e}")