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

url = "https://www.unicode.org/Public/idna/15.1.0/IdnaMappingTable.txt"
filename = "IdnaMappingTable.txt"
def get_table():
    if(not os.path.exists(filename)):
        tablefile = requests.get(url)
        with open(filename,  'wb') as file:
            file.write(tablefile.content)
    with open(filename, 'r') as file:
       return file.read()

def get_version(table_data):
    return re.search("# Version: (.*)", table_data).group(1)

def find_slice(seq, subseq):
    n = len(seq)
    m = len(subseq)
    for i in range(n - m + 1):
        if seq[i:i + m] == subseq:
            return i
    return -1

def values(words):
    answer = []
    for w in words:
        w2 = w[1]
        if(w[1] == 3):
            w2 += (w[2][0] << 8) + (w[2][1] << 24)
        answer.append([w[0][0], w2])
    return answer

UseSTD3ASCIIRules=False
def print_idna():
    table_data = get_table()
    print("// IDNA ", get_version(get_table()))
    words = []
    long_mapped = []
    previous_code = -1
    mapping = {"ignored":0,"valid":1,"disallowed":2,"mapped":3}
    for line in table_data.split("\n"):
        if(line.startswith("#") or ";" not in line):
            continue
        line = line[0:line.index("#")]
        line = line.split(";")
        line = [x.strip() for x in line if x.strip() != ""]
        if(not UseSTD3ASCIIRules and line[1].startswith("disallowed_STD3_")): line[1] = line[1][16:]
        if(line[1] == "deviation"): line[1] = "valid" # IDNA2008, see https://www.unicode.org/faq/idn.html
        codepoints = [int(x,16) for x in line[0].split("..")]
        code = mapping[line[1]]
        mapped = []
        if((code != 3) and (code == previous_code)):
            continue # nothing to do
        previous_code = code
        if(code == 3):
            mapped = [int(x,16) for x in line[2].split(" ")]
            already = find_slice(long_mapped, mapped)
            if already != -1:
                long_index = already
            else:
                long_index = len(long_mapped)
                long_mapped += mapped
            long_length = len(mapped)
            words.append([codepoints, code, [long_index, long_length]])
        else:
            words.append([codepoints, code])
    print("""
// clang-format off
#ifndef ADA_IDNA_TABLES_H
#define ADA_IDNA_TABLES_H
#include <cstdint>

namespace ada::idna {
""")
    print("const uint32_t mappings["+str(len(long_mapped))+"] =")
    print(multiline_cpp_array_initializer(long_mapped), end=";\n")
    print("const uint32_t table["+str(len(words))+"][2] =")
    print(cpp_arrayarray_initializer(values(words)), end=";\n")
    print("""

} // namespace ada::idna
#endif // ADA_IDNA_TABLES_H
""")

if __name__ == "__main__":
    print_idna()
