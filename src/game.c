/*	Copyright (C) 2017 David Leiter
 *	This program is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 3 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "game.h"

#include <stdlib.h>

static void initField(struct Game *game);

static void resetPos(struct Game *game);

static void generatePiece(struct PieceState *state);

static bool checkCellCollision(const int8_t grid[GRID_HEIGHT][GRID_WIDTH],
                               struct Pos pos);

static bool checkCollision(const struct Game *game, const struct Pos *pos,
                           int rotation);

static bool move(struct Game *game, int dir);

static void moveDown(struct Game *game);

static void lockPiece(struct Game *game);

static void updateStatistics(struct Game *game, int lines);

static void clearLines(struct Game *game);

static const struct Piece pieces[7] = {
    // O
    {1,
     {{{{0, 0}, {0, -1}, {1, -1}, {1, 0}}},
      {{{0, 0}, {0, -1}, {1, -1}, {1, 0}}},
      {{{0, 0}, {0, -1}, {1, -1}, {1, 0}}},
      {{{0, 0}, {0, -1}, {1, -1}, {1, 0}}}}},
    // I
    {2,
     {{{{-1, 0}, {0, 0}, {1, 0}, {2, 0}}},
      {{{0, -2}, {0, -1}, {0, 0}, {0, 1}}},
      {{{-1, 0}, {0, 0}, {1, 0}, {2, 0}}},
      {{{0, -2}, {0, -1}, {0, 0}, {0, 1}}}}},
    // J
    {4,
     {{{{-1, 0}, {0, 0}, {1, 0}, {1, 1}}},
      {{{-1, 1}, {0, 1}, {0, 0}, {0, -1}}},
      {{{-1, -1}, {-1, 0}, {0, 0}, {1, 0}}},
      {{{1, -1}, {0, -1}, {0, 0}, {0, 1}}}}},
    // L
    {4,
     {{{{-1, 1}, {-1, 0}, {0, 0}, {1, 0}}},
      {{{-1, -1}, {0, -1}, {0, 0}, {0, 1}}},
      {{{-1, 0}, {0, 0}, {1, 0}, {1, -1}}},
      {{{0, -1}, {0, 0}, {0, 1}, {1, 1}}}}},
    // S
    {4,
     {{{{-1, 1}, {0, 1}, {0, 0}, {1, 0}}},
      {{{-1, -1}, {-1, 0}, {0, 0}, {0, 1}}},
      {{{-1, 1}, {0, 1}, {0, 0}, {1, 0}}},
      {{{-1, -1}, {-1, 0}, {0, 0}, {0, 1}}}}},
    // T
    {4,
     {{{{-1, 0}, {0, 0}, {1, 0}, {0, 1}}},
      {{{-1, 0}, {0, -1}, {0, 0}, {0, 1}}},
      {{{-1, 0}, {0, 0}, {1, 0}, {0, -1}}},
      {{{0, -1}, {0, 0}, {0, 1}, {1, 0}}}}},
    // Z
    {4,
     {{{{-1, 0}, {0, 0}, {0, 1}, {1, 1}}},
      {{{-1, 1}, {-1, 0}, {0, 0}, {0, -1}}},
      {{{-1, 0}, {0, 0}, {0, 1}, {1, 1}}},
      {{{-1, 1}, {-1, 0}, {0, 0}, {0, -1}}}}}};

static void initField(struct Game *game)
{
	for (int y = 0; y < GRID_HEIGHT; y++) {
		for (int x = 0; x < GRID_WIDTH; x++) {
			game->field[y][x] = 0;
		}
	}
}

static size_t randomIndex(size_t size)
{
	size_t bucket_size = ((size_t)RAND_MAX + 1) / size;
	size_t max = bucket_size * size;
	size_t val = 0;
	do {
		val = (size_t)rand();
	} while (val >= max);
	return val / bucket_size;
}

static void generatePiece(struct PieceState *state)
{
	state->piece = randomIndex(7);
	state->rotation = randomIndex(4);
}

static void resetPos(struct Game *game)
{
	game->piece_pos.x = GRID_WIDTH / 2;
	game->piece_pos.y = 1;
}

void gameInit(struct Game *game)
{
	initField(game);
	game->key_left = false;
	game->key_right = false;
	game->key_down = false;
	game->rotate_right = false;
	game->rotate_left = false;
	game->level = 0;
	game->lines = 0;
	game->score = 0;
	game->points[0] = 40;
	game->points[1] = 100;
	game->points[2] = 300;
	game->points[3] = 1200;
	game->pieces = pieces;
	generatePiece(&game->current_piece);
	generatePiece(&game->next_piece);
	resetPos(game);
}

static bool checkCellCollision(const int8_t grid[GRID_HEIGHT][GRID_WIDTH],
                               struct Pos pos)
{
	if ((pos.x < 0) || (pos.x >= GRID_WIDTH) || (pos.y >= GRID_HEIGHT)) {
		return true;
	}
	if (grid[pos.y][pos.x] == 0) {
		return false;
	}

	return true;
}

static bool checkCollision(const struct Game *game, const struct Pos *pos,
                           int rotation)
{
	const struct Pos *blocks =
	    pieces[game->current_piece.piece].rotations[rotation].blocks;
	for (int i = 0; i < 4; i++) {
		struct Pos block = {(int8_t)(blocks[i].x + pos->x),
		                    (int8_t)(blocks[i].y + pos->y)};
		if (checkCellCollision(game->field, block)) {
			return true;
		}
	}
	return false;
}

static bool move(struct Game *game, int dir)
{
	struct Pos pos = game->piece_pos;
	switch (dir) {
		case DIRECTION_DOWN:
			pos.y++;
			break;
		case DIRECTION_LEFT:
			pos.x--;
			break;
		case DIRECTION_RIGHT:
			pos.x++;
			break;
	}
	const struct Pos *blocks = pieces[game->current_piece.piece]
	                               .rotations[game->current_piece.rotation]
	                               .blocks;
	for (int i = 0; i < 4; i++) {
		struct Pos block = {(int8_t)(blocks[i].x + pos.x),
		                    (int8_t)(blocks[i].y + pos.y)};
		if (checkCellCollision(game->field, block) == true) {
			return false;
		}
	}
	game->piece_pos = pos;
	return true;
}

static void lockPiece(struct Game *game)
{
	const struct Pos *blocks = pieces[game->current_piece.piece]
	                               .rotations[game->current_piece.rotation]
	                               .blocks;

	for (int i = 0; i < 4; i++) {
		struct Pos pos = {(int8_t)(blocks[i].x + game->piece_pos.x),
		                  (int8_t)(blocks[i].y + game->piece_pos.y)};
		game->field[pos.y][pos.x] = game->current_piece.piece + 1;
	}
}

static void moveDown(struct Game *game)
{
	if (move(game, DIRECTION_DOWN) == false) {
		lockPiece(game);
		clearLines(game);
		game->current_piece = game->next_piece;
		generatePiece(&game->next_piece);
		resetPos(game);
	}
}

static void rotate_right(struct Game *game)
{
	int count = pieces[game->current_piece.piece].count;
	int rotation = (game->current_piece.rotation + 1) % count;
	if (!checkCollision(game, &game->piece_pos, rotation)) {
		game->current_piece.rotation = rotation;
	}
}
static void rotate_left(struct Game *game)
{
	int count = pieces[game->current_piece.piece].count;
	int rotation = (game->current_piece.rotation + 3) % count;
	if (!checkCollision(game, &game->piece_pos, rotation)) {
		game->current_piece.rotation = rotation;
	}
}

static void moveGridDown(struct Game *game, int top, int count)
{
	for (int y = top; y >= 0; y--) {
		for (int x = 0; x < GRID_WIDTH; x++) {
			game->field[y + count][x] = game->field[y][x];
			game->field[y][x] = 0;
		}
	}
}

static void clearLines(struct Game *game)
{
	int count = 0;
	for (int y = GRID_HEIGHT - 1; y >= 0; y--) {
		bool line_full = true;
		for (int x = 0; x < GRID_WIDTH; x++) {
			if (game->field[y][x] == 0) {
				line_full = false;
				break;
			}
		}

		if (line_full) {
			count++;
		} else {
			if (count > 0) {
				moveGridDown(game, y, count);
				updateStatistics(game, count);
				count = 0;
			}
		}
	}
}

static void updateStatistics(struct Game *game, int lines)
{
	game->lines += lines;
	int base = game->points[lines - 1];
	game->score += (base * (game->level + 1));
}

void gameUpdate(struct Game *game)
{
	if (game->rotate_right) {
		rotate_right(game);
		game->rotate_right = false;
	}
	if (game->rotate_left) {
		rotate_left(game);
		game->rotate_left = false;
	}
	if (game->interval_count % MOVE_INTERVAL_COUNT == 0) {
		if (game->key_left) {
			move(game, DIRECTION_LEFT);
		} else if (game->key_right) {
			move(game, DIRECTION_RIGHT);
		}
	}
	if ((game->interval_count % SOFT_DROP_INTERVAL_COUNT == 0) &&
	    game->key_down) {
		moveDown(game);
	} else if (game->interval_count % DROP_INTERVAL_COUNT == 0) {
		moveDown(game);
	}
	// game->interval_count= (game->interval_count+1)%
	game->interval_count++;
}
