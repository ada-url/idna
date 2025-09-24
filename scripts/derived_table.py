import os.path
import requests
import re
import textwrap
import sys
if sys.version_info[0] < 3:
    print('You need to run this with Python 3')
    sys.exit(1)

indent = ' ' * 4

def fill(text):
    tmp = textwrap.fill(text)
    return textwrap.indent(tmp, indent)

def filltab(text):
    tmp = textwrap.fill(text, width=120)
    return textwrap.indent(tmp, '\t')

def cpp_array_initializer(arr):
    return '{%s}' % (', '.join(map(str, arr)))

def multiline_cpp_array_initializer(arr):
    alllines = '{\n'
    current_line = '\t'
    for i in arr:
        if(len(current_line) > 80):
            if(current_line[-1] == ' '):
                current_line = current_line[:-1]
            alllines += current_line + '\n'
            current_line = '\t'
        current_line += str(i)
        if(i != len(arr)-1): current_line += ', '
    if(len(current_line) > 0):
        if(current_line[-1] == ' '):
          current_line = current_line[:-1]
        alllines += current_line + '\n'
    alllines += '\n}'
    return alllines

def compose2(f, g):
    return lambda x: f(g(x))

def cpp_arrayarray_initializer(arr):
    alllines = '{'
    for i in range(0,len(arr), 4):
        alllines += '\n\t' + ', '.join(map(cpp_array_initializer,arr[i:i+4]))
        if(i + 4 < len(arr)): alllines += ','
    alllines += '\n}'
    return alllines


urlderived = "https://www.unicode.org/Public/17.0.0/ucd/DerivedCoreProperties.txt"
derivedfilename = "DerivedCoreProperties.txt"
def get_derived_table():
    if(not os.path.exists(derivedfilename)):
        tablefile = requests.get(urlderived)
        with open(derivedfilename,  'wb') as file:
            file.write(tablefile.content)
    with open(derivedfilename, 'r') as file:
       return file.read()

def get_version(table_data):
    return re.search(r"# DerivedCoreProperties-(.*)\.txt", table_data).group(1)


def parse_id_start_section(text):
    start_pattern = re.compile(r'# Derived Property: ID_Start')
    end_pattern = re.compile(r'# Derived Property: ID_Continue')
    
    # Find the start and end indices of the section
    start = start_pattern.search(text)
    end = end_pattern.search(text)
    
    if start is None or end is None:
        raise ValueError("Section markers not found in the text.")
    
    # Extract the relevant section
    section = text[start.end():end.start()].strip().split('\n')
    result = []
    for line in section:
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        
        parts = line.split(';')[0].strip().split('..')
        
        # Convert hex to int and handle single value
        if len(parts) == 1:
            start_value = int(parts[0], 16)
            result.append([start_value, start_value])
        elif len(parts) == 2:
            start_value = int(parts[0], 16)
            end_value = int(parts[1], 16)
            result.append([start_value, end_value])
        else:
            print(f"Warning: Line format not recognized: {line}")
    
    return result


def parse_id_continue_section(text):
    start_pattern = re.compile(r'# Derived Property: ID_Continue')
    end_pattern = re.compile(r'# Derived Property: XID_Start')
    
    # Find the start and end indices of the section
    start = start_pattern.search(text)
    end = end_pattern.search(text)
    
    if start is None or end is None:
        raise ValueError("Section markers not found in the text.")
    
    # Extract the relevant section
    section = text[start.end():end.start()].strip().split('\n')
    result = []
    for line in section:
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        
        parts = line.split(';')[0].strip().split('..')
        
        # Convert hex to int and handle single value
        if len(parts) == 1:
            start_value = int(parts[0], 16)
            result.append([start_value, start_value])
        elif len(parts) == 2:
            start_value = int(parts[0], 16)
            end_value = int(parts[1], 16)
            result.append([start_value, end_value])
        else:
            print(f"Warning: Line format not recognized: {line}")
    
    return result

def print_derived():
    table_data = get_derived_table()
    print("// IDNA ", get_version(get_derived_table()))
    id_start = parse_id_start_section(table_data)
    id_continue = parse_id_continue_section(table_data)
    print("""
// clang-format off
#ifndef ADA_IDNA_IDENTIFIER_TABLES_H
#define ADA_IDNA_IDENTIFIER_TABLES_H
#include <cstdint>

namespace ada::idna {
""")
    print("const uint32_t id_continue["+str(len(id_continue))+"][2] =")
    print(cpp_arrayarray_initializer(id_continue), end=";\n")
    print("const uint32_t id_start["+str(len(id_start))+"][2] =")
    print(cpp_arrayarray_initializer(id_start), end=";\n")
    print("""

} // namespace ada::idna
#endif // ADA_IDNA_IDENTIFIER_TABLES_H
""")
if __name__ == "__main__":
    print_derived()
