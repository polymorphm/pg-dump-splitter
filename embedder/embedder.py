#!/usr/bin/env python3

import sys
import os.path

def main():
    if len(sys.argv) != 5:
        raise AssertionError('len(sys.argv) != 5')

    plainname = sys.argv[1]
    input_path = sys.argv[2]
    output_c_path = sys.argv[3]
    output_h_path = sys.argv[4]

    if '"' in plainname:
        raise AssertionError('\'"\' in plainname')

    ident = plainname.replace('.', '_').replace('-', '_').upper()

    with open(input_path, 'rb') as in_fd, \
            open(output_c_path, 'w', encoding='utf-8', newline='\n') as out_c_fd, \
            open(output_h_path, 'w', encoding='utf-8', newline='\n') as out_h_fd:
        out_c_fd.write(
            '#include "{}"\n\n'
            'const char EMBEDDED_{}_DATA[] = {{'.format(
                os.path.basename(output_h_path),
                ident,
            ),
        )

        size = 0

        while True:
            data = in_fd.read(16)

            if not data:
                break

            size += len(data)
            out_c_fd.write('\n')
            first_char = True

            for b in data:
                if not first_char:
                    out_c_fd.write(' ')
                    first_char = False

                i = int(b)

                if i >= 128:
                    i -= 256

                out_c_fd.write('{},'.format(i))

        out_c_fd.write(
            '\n0,\n}};\n\nunsigned long EMBEDDED_{}_SIZE = {};\n'.format(
                ident,
                size,
            ),
        )

        out_h_fd.write(
            'extern const char EMBEDDED_{}_DATA[];\n'
            'extern unsigned long EMBEDDED_{}_SIZE;\n'.format(
                ident,
                ident,
            ),
        )

if __name__ == '__main__':
    main()

# vi:ts=4:sw=4:et
