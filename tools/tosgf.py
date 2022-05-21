import argparse
import sys

def tosgf(source=sys.stdin):
    trans_infos=''
    for info in source:
        info = info[info.find('(')+1:info.find(')')] # remove ()
        trans_info = '(;FF[4]'
        l_bracket = info.find('[')
        r_bracket = info.find(']')
        board_sz = 0
        while l_bracket != -1 and r_bracket != -1:
            label = info[:l_bracket]
            content = info[l_bracket+1:r_bracket]
            if label == 'B' or label == 'W':
                if board_sz <= 0:
                    raise ValueError('Invalid board size!')
                label = ';'+label
                content = content.split('|')[0]
                position = int(content)
                if position == board_sz**2:
                    content = ''
                else:
                    content = chr(ord('a')+position % board_sz) + \
                        chr(ord('a')+(board_sz-1-position//board_sz))
            elif label == 'SZ':
                board_sz = int(content)
            elif label == 'GM' and content == 'go':
                content = '1'
            trans_info += f'{label}[{content}]'
            info = info[r_bracket+1:]
            l_bracket = info.find('[')
            r_bracket = info.find(']')
        trans_info += ')\n'
        trans_infos+=trans_info
    return trans_infos


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-i', dest='fin_name', type=str,
                         help='input flie')
    parser.add_argument('-o', dest='fout_name', type=str,
                         help='output flie')
    args = parser.parse_args()
    trans_infos=''
    if args.fin_name:
        try:
            with open(args.fin_name, 'r') as fin:
                trans_infos+=tosgf(fin.readlines())
        except:
            print(f'\"{args.fin_name}\" does not exist!')
            exit(1)
    else:
        trans_infos+=tosgf(sys.stdin)
    if args.fout_name:
        with open(args.fout_name, 'w') as fout:
            fout.write(trans_infos)
        print(f'Writed to {args.fout_name}.')
    else:
        print(trans_infos)
