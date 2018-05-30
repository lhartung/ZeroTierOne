from sys import argv
from re import sub, finditer, VERBOSE

def gen(defs):
    indent = 0
    enum = False
    def p(s): print(" " * (indent * 4) + s)
    for item in finditer("""
        (?P<type> message|enum) \\s+ (?P<name> \\w+) \\s* \\{ |
        (?P<var> \\w+) \\s* = \\s* (?P<val> \\w+) \\s* ; |
        \\}
        """, defs, flags=VERBOSE):
        if item.group(0) == "}":
            indent = indent - 1
            p("};" if enum else "}")
            enum = False;
        elif item.group('type') == 'enum':
            p("enum class %s {" % item.group('name'))
            indent = indent + 1
            enum = True
        elif item.group('type') == 'message':
            p("namespace %s {" % item.group('name'))
            indent = indent + 1
            enum = False
        else:
            if enum:
                p("%s = %s," % (item.group('var'), item.group('val')))

print("// Auto-generated by reql/gen.py")
print("#pragma once")
print("namespace RethinkDB { namespace Protocol {")
gen(sub("//.*", "", open(argv[1]).read()))
print("} }")
