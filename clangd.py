#!/usr/bin/env python3
import os
import json
import shutil

def merge_compile_commands():
    # 创建build目录(如果不存在)
    if not os.path.exists('build'):
        os.makedirs('build')

    # 存储所有编译命令
    all_commands = []

    # 遍历当前目录及子目录
    for root, dirs, files in os.walk('.'):
        if 'compile_commands.json' in files:
            json_path = os.path.join(root, 'compile_commands.json')
            
            try:
                with open(json_path, 'r') as f:
                    commands = json.load(f)
                    if isinstance(commands, list):
                        all_commands.extend(commands)
                        print(f"已处理文件: {json_path}")
            except Exception as e:
                print(f"处理文件 {json_path} 时出错: {str(e)}")

    # 如果找到了编译命令
    if all_commands:
        output_path = os.path.join('build', 'compile_commands.json')
        try:
            with open(output_path, 'w') as f:
                json.dump(all_commands, f, indent=2)
            print(f"已成功合并编译命令到 {output_path}")
        except Exception as e:
            print(f"写入合并文件时出错: {str(e)}")
    else:
        print("未找到任何compile_commands.json文件")

if __name__ == '__main__':
    merge_compile_commands()
