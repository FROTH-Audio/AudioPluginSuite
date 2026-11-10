with open('fix_tapestop2.py', 'r') as f:
    content = f.read()
content = content.replace('new_processBlock', 'new_func')
with open('fix_tapestop2.py', 'w') as f:
    f.write(content)
print('Fixed variable name')