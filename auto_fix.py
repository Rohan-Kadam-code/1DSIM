import os
import re
import subprocess

def auto_fix():
    for iteration in range(15):
        result = subprocess.run(['python', 'build_desktop.py'], capture_output=True, text=True)
        if result.returncode == 0:
            print("Build succeeded!")
            break

        lines = result.stdout.split('\n')
        
        with open('src/desktop/main.cpp', 'r') as f:
            content_lines = f.read().split('\n')

        fixed = 0
        for line in lines:
            m = re.search(r'src\\desktop\\main\.cpp\((\d+)\): error C2039: \'(.*?)\': is not a member of \'std::shared_ptr<(DesktopNode|DesktopLink)>\'', line)
            if m:
                line_num = int(m.group(1)) - 1
                prop = m.group(2)
                old_line = content_lines[line_num]
                # Replace `.prop` with `->prop` on that line
                new_line = re.sub(rf'\b(\w+)\.{prop}\b', rf'\1->{prop}', old_line)
                
                # If the property wasn't replaced, try a broader regex (e.g. function returns like iterators)
                if old_line == new_line:
                    new_line = old_line.replace(f'.{prop}', f'->{prop}')
                    
                if old_line != new_line:
                    content_lines[line_num] = new_line
                    fixed += 1

            # Match: cannot convert from 'std::shared_ptr<DesktopNode>' to 'DesktopNode'
            m2 = re.search(r'src\\desktop\\main\.cpp\((\d+)\): error C2440:', line)
            if m2 and "cannot convert from 'std::shared_ptr<DesktopNode>' to 'DesktopNode'" in line:
                line_num = int(m2.group(1)) - 1
                old_line = content_lines[line_num]
                # Usually occurs in auto n : g_nodes when expecting value type but getting ptr?
                # Actually auto handles it. The error might be assigning shared_ptr to DesktopNode.
                
        # Additionally, just comment out all uses of g_comp_instances and g_comp_connections because they are obsolete
        for i in range(len(content_lines)):
            if not content_lines[i].strip().startswith('//'):
                if 'g_comp_instances' in content_lines[i] or 'g_comp_connections' in content_lines[i] or 'g_sel_comp' in content_lines[i] or 'g_sel_conn' in content_lines[i]:
                    content_lines[i] = '// ' + content_lines[i]
                    fixed += 1

        with open('src/desktop/main.cpp', 'w') as f:
            f.write('\n'.join(content_lines))

        print(f"Iteration {iteration}: Fixed {fixed} errors.")
        if fixed == 0:
            print("Could not auto fix any more errors.")
            print('\n'.join(lines[:100]))
            break

if __name__ == '__main__':
    auto_fix()
