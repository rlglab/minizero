#!/usr/bin/env python

import argparse
import sys
import os

gm_map = {'go': '1',
          'hex': '11'}


def getGame(content):
    if content in gm_map:
        return gm_map[content]
    print(f'Not supported game type: {content}', file=sys.stderr)
    print(f'Supported game types: {list(gm_map.keys())}', file=sys.stderr)
    exit(1)


def tosgf(source):
    trans_infos = ''
    for info in source:
        info = info[info.find('(') + 1:info.find(')')]  # remove ( )
        trans_info = '(;FF[4]'
        l_bracket = info.find('[')
        r_bracket = info.find(']')
        board_sz = 0
        while l_bracket != -1 and r_bracket != -1:
            label = info[:l_bracket]
            content = info[l_bracket + 1:r_bracket]
            if label[0] == ';':  # new format
                label = label[1:]
            if label == 'B' or label == 'W':
                if board_sz <= 0:
                    raise ValueError('Invalid board size!')
                label = ';' + label
                content = content.split('|')[0].split(']')[0]
                position = int(content)
                if position == board_sz**2:
                    content = ''
                else:
                    content = chr(ord('a') + position % board_sz) + \
                        chr(ord('a') + (board_sz - 1 - position // board_sz))
            elif label == 'SZ':
                board_sz = int(content)
            elif label == 'GM':
                content = getGame(content.split('_')[0])
            trans_info += f'{label}[{content}]'
            info = info[r_bracket + 1:]
            l_bracket = info.find('[')
            r_bracket = info.find(']')
        trans_info += ')\n'
        trans_infos += trans_info
    return trans_infos


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-in_file', dest='fin_name', type=str,
                        help='input flie')
    parser.add_argument('-out_file', dest='fout_name', type=str,
                        help='output flie')
    parser.add_argument('--force', action='store_true',
                        dest='force', help='overwrite files')
    args = parser.parse_args()
    if args.fin_name:
        if os.path.isfile(args.fin_name):
            with open(args.fin_name, 'r') as fin:
                trans_infos = tosgf(fin.readlines())
        else:
            print(f'\"{args.fin_name}\" does not exist!', file=sys.stderr)
            exit(1)
    else:
        trans_infos = tosgf(sys.stdin)
    if args.fout_name:
        if not args.force and os.path.isfile(args.fout_name):
            print(
                f'*** {args.fout_name} exists! Use --force to overwrite it. ***')
            exit(1)
        with open(args.fout_name, 'w') as fout:
            fout.write(trans_infos)
        print(f'Writed to {args.fout_name}.')
    else:
        print(trans_infos)
