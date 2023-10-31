from matplotlib.patches import RegularPolygon
import matplotlib.patches as patches
import matplotlib.pyplot as plt
from PIL import Image
import numpy as np
import argparse
import os

class PlotBoard:
    def plot_board(game, boardsize, blackboard, whiteboard, cardinality = None, addtext = None):
        assert(len(blackboard) == len(whiteboard))

        plt.rc('font', family='monospace')

        if game == 'go' or game == 'gomoku' or game == 'killallgo' or game == 'nogo':
            PlotBoard.plot_go_board(boardsize, blackboard, whiteboard, cardinality, addtext)
        elif game == 'othello':
            PlotBoard.plot_othello_board(boardsize, blackboard, whiteboard, cardinality, addtext)
        elif game == 'hex':
            PlotBoard.plot_hex_board(boardsize, blackboard, whiteboard, cardinality, addtext)
        else:
            print(f"Unsupported game: {game}")
    def adjust_piece_color_ratio(blackvalue, whitevalue):
        sumvalue = blackvalue + whitevalue
        if (sumvalue > 1):
            return float(blackvalue/sumvalue), float(whitevalue/sumvalue)
        else:
            return blackvalue, whitevalue

    def plot_othello_board(boardsize, blackboard, whiteboard, cardinality = None, addtext = None):
        
        # setting
        wholeplot_figsize = (2.5, 2.5)
        adjusted_xratio = 0.85
        adjusted_yratio = 0.85
        if(addtext is not None):
            wholeplot_figsize = (wholeplot_figsize[0], wholeplot_figsize[1] + 0.3*len(addtext))
            adjusted_yratio = wholeplot_figsize[0]*adjusted_xratio/wholeplot_figsize[1]

        # create a board
        backgroundcolor = (51, 153, 102)
        fig, ax = plt.subplots(figsize=wholeplot_figsize)
        ax.set_facecolor(tuple(x / 255 for x in backgroundcolor))

        # draw the grid
        adjusted_linewidth = 1.0*(8/boardsize)
        for x in range(boardsize + 1):
            ax.plot([x, x], [0, boardsize], 'k', linewidth=adjusted_linewidth)
        for y in range(boardsize + 1):
            ax.plot([0, boardsize], [y, y], 'k', linewidth=adjusted_linewidth)

        # draw the discs (Othello pieces)
        for x in range(boardsize):
            for y in range(boardsize):
                whitepiecevalue = max(min(whiteboard[y*boardsize+x],1),0)
                blackpiecevalue = max(min(blackboard[y*boardsize+x],1),0)
                blackpiecevalue, whitepiecevalue = PlotBoard.adjust_piece_color_ratio(blackpiecevalue, whitepiecevalue)
                if (blackpiecevalue > whitepiecevalue):
                    ax.add_patch(plt.Circle((x + 0.5, y + 0.5), 0.4, fill=True, color='black', alpha=blackpiecevalue))
                    ax.add_patch(plt.Circle((x + 0.5, y + 0.5), 0.4, fill=True, color='white', alpha=whitepiecevalue))
                else:
                    ax.add_patch(plt.Circle((x + 0.5, y + 0.5), 0.4, fill=True, color='white', alpha=whitepiecevalue))
                    ax.add_patch(plt.Circle((x + 0.5, y + 0.5), 0.4, fill=True, color='black', alpha=blackpiecevalue))

        adjusted_fontsize = 10*(8/boardsize)
        if(cardinality is not None):
            for x in range(boardsize):
                for y in range(boardsize):
                    piecevalue = int(cardinality[y*boardsize+x])
                    if(piecevalue!=0):
                        if(piecevalue > 99):
                            ax.text(x + 0.5, y + 0.5, piecevalue, ha='center', va='center', fontsize=(adjusted_fontsize/2)+1, color='coral')
                        else:
                            ax.text(x + 0.5, y + 0.5, piecevalue, ha='center', va='center', fontsize=adjusted_fontsize, color='coral')

        # scale the axis area to fill the whole figure
        ax.set_position([0.1, (1-adjusted_yratio-0.05), adjusted_xratio, adjusted_yratio])

        # Set axis labels
        plt.yticks([element + 0.5 for element in list(range(boardsize))], [str(i) for i in range(1, boardsize + 1)])
        plt.xticks([element + 0.5 for element in list(range(boardsize))], [chr(ord('A') + i + (i >= 8)) for i in range(boardsize)])

        if(addtext is not None):
            fix_text_height = -0.1
            for text_item,text_idx in zip(addtext, range(1, len(addtext)+1)):
                ax.text(0.5, fix_text_height + fix_text_height*text_idx, text_item, transform=ax.transAxes,
                    fontsize=12, color='black', ha='center', va='center')
        
    def plot_go_board(boardsize, blackboard, whiteboard, cardinality = None, addtext = None):
        
        # setting
        wholeplot_figsize = (2.5, 2.5)
        adjusted_xratio = 0.85
        adjusted_yratio = 0.85
        if(addtext is not None):
            wholeplot_figsize = (wholeplot_figsize[0], wholeplot_figsize[1] + 0.3*len(addtext))
            adjusted_yratio = wholeplot_figsize[0]*adjusted_xratio/wholeplot_figsize[1]

        # create a board
        fig, ax = plt.subplots(figsize=wholeplot_figsize)
        ax.set_facecolor('#DDBB99')

        # draw the grid
        adjusted_linewidth = 1.0*(8/boardsize)
        for x in range(boardsize):
            ax.plot([x, x], [0,boardsize-1], 'k', zorder=0, linewidth=adjusted_linewidth)
        for y in range(boardsize):
            ax.plot([0, boardsize-1], [y,y], 'k', zorder=0, linewidth=adjusted_linewidth)

        # scale the axis area to fill the whole figure
        ax.set_position([0.1, (1-adjusted_yratio-0.05), adjusted_xratio, adjusted_yratio])

        # Set axis labels
        plt.yticks(range(boardsize), [str(i) for i in range(1, boardsize + 1)])
        plt.xticks(range(boardsize), [chr(ord('A') + i + (i >= 8)) for i in range(boardsize)])
        
        # draw the discs (Go pieces)
        for x in range(boardsize):
            for y in range(boardsize):
                whitepiecevalue = max(min(whiteboard[y*boardsize+x],1),0)
                blackpiecevalue = max(min(blackboard[y*boardsize+x],1),0)
                blackpiecevalue, whitepiecevalue = PlotBoard.adjust_piece_color_ratio(blackpiecevalue, whitepiecevalue)
                if (blackpiecevalue > whitepiecevalue):
                    ax.add_patch(plt.Circle((x, y), 0.4, fill=True, color='black', alpha=blackpiecevalue))
                    ax.add_patch(plt.Circle((x, y), 0.4, fill=True, color='white', alpha=whitepiecevalue))
                else:
                    ax.add_patch(plt.Circle((x, y), 0.4, fill=True, color='white', alpha=whitepiecevalue))
                    ax.add_patch(plt.Circle((x, y), 0.4, fill=True, color='black', alpha=blackpiecevalue))


        adjusted_fontsize = 10*(8/boardsize)
        if(cardinality is not None):
            for x in range(boardsize):
                for y in range(boardsize):
                    piecevalue = int(cardinality[y*boardsize+x])
                    if(piecevalue!=0):
                        if(piecevalue > 99):
                            ax.text(x, y, piecevalue, ha='center', va='center', fontsize=(adjusted_fontsize/2+1), color='coral')
                        else:
                            ax.text(x, y, piecevalue, ha='center', va='center', fontsize=adjusted_fontsize, color='coral')

        if(addtext is not None):
            fix_text_height = -0.1
            for text_item,text_idx in zip(addtext, range(1, len(addtext)+1)):
                ax.text(0.5, fix_text_height + fix_text_height*text_idx, text_item, transform=ax.transAxes,
                    fontsize=12, color='black', ha='center', va='center')
            
    def plot_hex_board(boardsize, blackboard, whiteboard, cardinality = None, addtext = None, border = True):
        board = []
        for x in range(boardsize):
            for y in range(boardsize):
                board.append((x,y*2))
        
        coord_tuples = PlotBoard.transform_cartesian_coords(board)
        
        # setting
        wholeplot_figsize = (2, 1.4)
        adjusted_xratio = 0.9
        adjusted_yratio = 0.9
        if(addtext is not None):
            wholeplot_figsize = (wholeplot_figsize[0], wholeplot_figsize[1] + 0.12*len(addtext))
            adjusted_yratio = (1.4/2)*wholeplot_figsize[0]*adjusted_xratio/wholeplot_figsize[1]
        fig, ax = plt.subplots(1, figsize=wholeplot_figsize, dpi=300)
        ax.set_position([0.05, (1-adjusted_yratio-0.05), adjusted_xratio, adjusted_yratio])
        x_coord_tuples = [c[0] for c in coord_tuples]
        y_coord_tuples = [c[1] for c in coord_tuples]
        ax.set_ylim(bottom=min(y_coord_tuples) - 2, top=max(y_coord_tuples) + 2)

        #board color
        boardcolor = '#DDBB99'
        
        # Hide the axes
        ax.set_xticks([])
        ax.set_yticks([])
        
        #set bg color and keep shape
        ax.set_facecolor(boardcolor)
        ax.set_aspect('equal')
        for spine in ax.spines.values():
            spine.set_visible(False)

        # Add some coloured hexagons
        adjusted_linewidth = 0.5*(11/boardsize)
        radius = 2. / 3
        
        border_edge_width = 0.2
        for x in range(boardsize):
            PlotBoard.draw_half_hexagon(coord_tuples[x*boardsize], radius+border_edge_width, 90, color="black")
        
        for x in range(boardsize):
            PlotBoard.draw_half_hexagon(coord_tuples[(boardsize-1)*boardsize+x], radius+border_edge_width, 30, color="white")
        
        for x in range(boardsize):
            PlotBoard.draw_half_hexagon(coord_tuples[x*boardsize+(boardsize-1)], radius+border_edge_width, 270, color="black")

        for x in range(boardsize):
            PlotBoard.draw_half_hexagon(coord_tuples[x], radius+border_edge_width, 210, color="white")
        
        PlotBoard.draw_rotated_rectangle(coord_tuples[0], radius+adjusted_linewidth/2.5, (radius)*2+adjusted_linewidth/2.5, -30, color="black")
        PlotBoard.draw_rotated_rectangle(coord_tuples[(boardsize-1)*boardsize+(boardsize-1)], radius+adjusted_linewidth/2.5, (radius)*2+adjusted_linewidth/2.5, -30, color="white")
        
        PlotBoard.draw_half_hexagon(coord_tuples[(boardsize-1)*boardsize], radius+border_edge_width, 150, color="black")
        PlotBoard.draw_half_hexagon(coord_tuples[boardsize-1], radius+border_edge_width, 330, color="black")
        
        for coord in coord_tuples:
            hex = RegularPolygon(coord, numVertices=6, radius=radius, 
                                orientation=np.radians(0),
                                edgecolor='k', linewidth=adjusted_linewidth, facecolor=boardcolor)
            ax.add_patch(hex)
            # Also add a text label
            
        # Also add scatter points in hexagon centres
        ax.scatter([c[0] for c in coord_tuples], [c[1] for c in coord_tuples], alpha=0)
        
        # add hex pieces
        for coord, i in zip(coord_tuples, range(boardsize*boardsize)):
            
            whitepiecevalue = max(min(whiteboard[i],1),0)
            blackpiecevalue = max(min(blackboard[i],1),0)
            blackpiecevalue, whitepiecevalue = PlotBoard.adjust_piece_color_ratio(blackpiecevalue, whitepiecevalue)
            if(blackpiecevalue > whitepiecevalue):
                ax.add_patch(plt.Circle(coord, 0.5, fill=True, color='black', alpha=blackpiecevalue, linewidth=0.))
                ax.add_patch(plt.Circle(coord, 0.5, fill=True, color='white', alpha=whitepiecevalue, linewidth=0.))
            else:
                ax.add_patch(plt.Circle(coord, 0.5, fill=True, color='white', alpha=whitepiecevalue, linewidth=0.))
                ax.add_patch(plt.Circle(coord, 0.5, fill=True, color='black', alpha=blackpiecevalue, linewidth=0.))

        adjusted_fontsize = 4*(11/boardsize)
        if(cardinality is not None):
            for x in range(boardsize):
                for y in range(boardsize):
                    piecevalue = int(cardinality[y*boardsize+x])
                    if(piecevalue!=0):
                        if(piecevalue > 99):
                            ax.text(coord_tuples[y*boardsize+x][0], y, piecevalue, ha='center', va='center', fontsize=(adjusted_fontsize/2+1), color='coral')
                        else:
                            ax.text(coord_tuples[y*boardsize+x][0], y, piecevalue, ha='center', va='center', fontsize=adjusted_fontsize, color='coral')

        # Set axis labels
        for y in range(boardsize):
            ax.text(coord_tuples[y*boardsize][0]-1.2, y, str(y+1), ha='center', va='center', fontsize=adjusted_fontsize-0.5, color='k')
        for x, t in zip(x_coord_tuples[:boardsize],[chr(ord('A') + i + (i >= 8)) for i in range(boardsize)]):
            ax.text(x, -1.2, t, ha='center', va='center', fontsize=adjusted_fontsize-0.5, color='k')
        
        if(addtext is not None):
            fix_text_height = -0.1
            for text_item,text_idx in zip(addtext, range(1, len(addtext)+1)):
                ax.text(0.5, fix_text_height + fix_text_height*text_idx+0.13, text_item, transform=ax.transAxes,
                    fontsize=6, color='black', ha='center', va='center')
        
    def transform_board_coord(appendboard, boardsize):
        board = []
        for x in range(boardsize):
            for y in range(boardsize):
                board.append((x,y*2,appendboard[x*boardsize+y]))
        return board

    def transform_cartesian_coords(board):
        # Horizontal cartesian coords
        hcoord = [2. * np.sin(np.radians(60)) * (c[1] - c[0]) / 3. for c in board]

        # Vertical cartesian coords
        vcoord = [c[0] for c in board]
        coord_tuples = []
        for h,v in zip(hcoord, vcoord):
            coord_tuples.append((h,v))
        return coord_tuples
    
    def draw_half_hexagon(center, size, angle, color="blue"):

        # Define the vertices of the half hexagon
        vertices = [(np.cos(np.radians(angle + 60 * i)) * size + center[0],
                     np.sin(np.radians(angle + 60 * i)) * size + center[1])
                    for i in range(4)]

        # Create a Polygon patch for the half hexagon
        from matplotlib.patches import Polygon
        hexagon = Polygon(vertices, closed=True, edgecolor=None, facecolor=color)

        # Plot the half hexagon
        plt.gca().add_patch(hexagon)
    
    def draw_rotated_rectangle(center, width, height, angle, color="blue"):

        angle = np.radians(angle)  # Rotation angle in radians

        # Create a rotation matrix
        rotation_matrix = np.array([[np.cos(angle), -np.sin(angle)],
                                    [np.sin(angle), np.cos(angle)]])

        # Define the vertices of the unrotated rectangle
        half_width = width / 2
        half_height = height / 2
        vertices = np.array([[-half_width, -half_height],
                             [0, -half_height],
                             [0, half_height],
                             [-half_width, half_height]])

        # Apply the rotation to the vertices
        rotated_vertices = np.dot(vertices, rotation_matrix.T) + center

        # Create a Polygon patch for the rotated rectangle
        rotated_rectangle = patches.Polygon(rotated_vertices, closed=True, edgecolor=None, facecolor=color)

        # Plot the rotated rectangle
        plt.gca().add_patch(rotated_rectangle)

    def savefig(out_dir, out_file = None, game = ''):
        if(out_file is not None):
            if out_file.endswith(".png"):
                plt.savefig(f'{out_dir}/{out_file}', dpi=300)
            elif(out_file.endswith(".svg")):
                plt.savefig(f'{out_dir}/{out_file}')
            else:
                print("The filename extension must be \'.png\' or \'.svg\'.")
        else:
            if out_dir.endswith(".png"):
                plt.savefig(f'{out_dir}', dpi=300)
            elif out_dir.endswith(".svg"):
                plt.savefig(f'{out_dir}')
            else:
                save_name = 'board.svg' if(game == '') else f'{game}-board.svg'
                plt.savefig(f'{out_dir}/{save_name}')
        plt.close()

    def delete_png_file(file_path):
        try:
            os.remove(file_path)
            # print(f"The file {file_path} has been deleted.")
        except FileNotFoundError:
            print(f"The file {file_path} does not exist.")
        except Exception as e:
            print(f"An error occurred: {e}")

    def merge_images_vertical(image_path1, image_path2, output_path):
        image1 = Image.open(image_path1)
        image2 = Image.open(image_path2)

        width, height = image1.size

        merged_image = Image.new('RGB', (width, height * 2))

        merged_image.paste(image1, (0, 0))

        merged_image.paste(image2, (0, height))

        merged_image.save(output_path)

        PlotBoard.delete_png_file(image_path1)

        PlotBoard.delete_png_file(image_path2)

    def merge_images_horizontal(image_paths, output_path):
        images = [Image.open(image_path) for image_path in image_paths]

        if(len(images) > 1):
            widths, heights = zip(*(i.size for i in images))

            total_width = sum(widths)
            max_height = max(heights)

            merged_image = Image.new('RGB', (total_width, max_height))

            x_offset = 0
            
            for image in images:
                merged_image.paste(image, (x_offset, 0))
                x_offset += image.width

            merged_image.save(output_path)

            for image_path in image_paths:
                PlotBoard.delete_png_file(image_path)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()

    parser.add_argument('-out_dir', dest='out_dir', default='.', help='output directory (default: in_dir)')
    parser.add_argument('-out_file', dest='out_file', default=None, help='output file name (default: board.svg)')
    parser.add_argument('-game', dest='game', default="go", type=str,
                        help='whcih game, default go')
    parser.add_argument('-boardsize', dest='boardsize', type=int,
                        help='the width of board')
    parser.add_argument('-black', nargs='+', type=float,
                        help='black pieces array. ex: -black 1 0 0 1 0 0 1 1 1 0 0 0 1 0 0 1 0')
    parser.add_argument('-white', nargs='+', type=float,
                        help='white pieces array. ex: -black 1 0 0 1 0 0 1 1 1 0 0 0 1 0 0 1 0')
    
    args = parser.parse_args()
    out_dir = args.out_dir
    out_dir = out_dir.rstrip('/')
    if args.black and args.white and args.boardsize:
        blackfeature = args.black
        whitefeature = args.white
        boardsize = args.boardsize
        game = args.game
        assert(len(blackfeature) == len(whitefeature))
        PlotBoard.plot_board(game, boardsize, blackfeature, whitefeature)
        PlotBoard.savefig(out_dir, args.out_file, args.game)
    else:
        parser.print_help()
        exit(1)