import numpy as np
import pandas as pd
import os
import argparse


def compute_elo(cur_elo, win):
    if win == 1:
        cur_elo += 1000
    elif win == 0:
        cur_elo -= 1000
    else:
        cur_elo -= max(min(1000, 400*np.log((1-win)/win)), -1000)
    return round(cur_elo, 3)


def getWinRate(file, is_white):
    with open(file, 'r') as f:
        lines = f.read()
        lines = lines.split('ERR_MSG\n')[1].split('\n')[:-1]
        total = len(lines)
        if is_white:
            results = np.array([-1*(-1)**idx*float(line.split('\t')[1])
                                for idx, line in enumerate(lines)])
            black_results = results[1::2]
            white_results = results[::2]
        else:
            results = np.array([(-1)**idx*float(line.split('\t')[1])
                                for idx, line in enumerate(lines)])
            black_results = results[::2]
            white_results = results[1::2]
        black_wins = black_results[black_results == 1].shape[0]
        white_wins = white_results[white_results == 1].shape[0]
        draws = results[results == 0].shape[0]
        win_rate = (
            results[results == 1].sum()+draws/2)/results.shape[0]
        return round(win_rate, 4), black_wins, white_wins, draws, total


def eval(dir, fout_name, player1_elo_file, is_white):
    wins = []
    black_wins = []
    white_wins = []
    draws = []
    totals = []
    player1s = []
    player2s = []
    for subdir in os.listdir(dir):
        datfile = os.path.join(dir, subdir, f'{subdir}.dat')
        if os.path.isfile(datfile):
            player12 = subdir.split('.')[0].split('_vs_')
            win, black_win, white_win, draw, total = getWinRate(
                datfile, is_white)
            wins.append(win)
            draws.append(draw)
            black_wins.append(black_win)
            white_wins.append(white_win)
            totals.append(total)
            player1s.append(int(player12[0]))
            if len(player12) > 1:
                player2s.append(int(player12[1]))
            else:
                player2s.append(int(player12[0]))
    result = pd.DataFrame({'P1': player1s,
                           'P2': player2s,
                           'Black': black_wins,
                           'White': white_wins,
                           'Draw': draws,
                           'Total': totals,
                           'WinRate': wins}, index=player1s).sort_values(['P2', 'P1'])
    if player1_elo_file:
        elo_df = pd.read_csv(player1_elo_file)
        black_elos = []
        white_elos = []
        for model, win in zip(result['P1'].iloc, result['WinRate'].iloc):
            try:
                if not is_white:
                    black_elos.append(
                        elo_df[elo_df['P1'] == model]['P1 Elo'].to_list()[0])
                    white_elos.append(compute_elo(black_elos[-1], 1-win))
                else:
                    white_elos.append(
                        elo_df[elo_df['P1'] == model]['P1 Elo'].to_list()[0])
                    black_elos.append(compute_elo(white_elos[-1], 1-win))
            except:
                black_elos.append(np.nan)
                white_elos.append(np.nan)
                print(f'No data for {model} in {player1_elo_file} !')
        result.insert(6, 'P1 Elo', black_elos)
        result.insert(7, 'P2 Elo', white_elos)

    else:
        cur_elo = 0
        elos = []
        for win in result['WinRate'].iloc:
            cur_elo = compute_elo(cur_elo, win)
            elos.append(cur_elo)
        result.insert(6, 'P1 Elo', elos)
    result.to_csv(os.path.join(dir, f'{fout_name}.csv'), index=False)
    print(result.to_string(index=False))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-d', '--dir', dest='dir', type=str,
                        help='dir')
    parser.add_argument('-o', '--out', dest='fout_name', type=str, default='elo',
                        help='output flie')
    parser.add_argument('-e', '--elo', dest='player1_elo_file', type=str,
                        help='elo flie of player 1')
    parser.add_argument('-w', '--white', action='store_true', dest='is_white',
                        help='output flie')
    args = parser.parse_args()
    if args.dir and os.path.isdir(args.dir):
        eval(args.dir, args.fout_name, args.player1_elo_file, args.is_white)
    else:
        print('No such directory!')
