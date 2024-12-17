import re

with open("640.txt", "r") as file:
    log_data = file.read()
# 定义正则表达式提取 Total Cycles
total_cycles_pattern = r"Total Cycles: (\d+)"

# 使用正则表达式找到所有匹配的 Total Cycles
total_cycles = [int(match) for match in re.findall(total_cycles_pattern, log_data)]

# 计算平均值
if total_cycles:
    average_cycles = sum(total_cycles) / len(total_cycles)
    print(f"Total Cycles 的平均值为: {average_cycles}")
else:
    print("未找到 Total Cycles 数据")
