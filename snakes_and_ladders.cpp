/*
 * =====================================================================
 *   SNAKES AND LADDERS  -  Data Structures and Algorithms Project
 * =====================================================================
 *
 *   Data structures and algorithms used in this program:
 *
 *   1. ARRAY        -> snakes[] and ladders[] store the start and end
 *                      squares of every snake and ladder.
 *   2. LINKED LIST  -> PlayerList stores the players (name, position,
 *                      moves, snakes hit, ladders climbed).
 *   3. QUEUE        -> TurnQueue decides whose turn it is (round-robin).
 *   4. STACK        -> MoveStack stores the move history. Undo pops the
 *                      last move from the top of the stack.
 *   5. SEARCHING    -> Binary search on the sorted snake/ladder arrays.
 *                      Linear search on the player list (to stop two
 *                      players from using the same name).
 *   6. SORTING      -> Selection sort puts the snake/ladder arrays in
 *                      order of start square (needed for binary search).
 *                      Bubble sort orders the leaderboard by moves.
 *
 *   Compile:  g++ snakes_and_ladders.cpp -o snakes_and_ladders
 *   Run:      ./snakes_and_ladders     (or snakes_and_ladders.exe)
 * =====================================================================
 */

#include <iostream>
#include <fstream>   // ifstream / ofstream for the leaderboard file
#include <string>
#include <cstdlib>   // rand, srand, atoi, exit
#include <ctime>     // time (used to seed the random number generator)
#include <iomanip>   // setw (used to line up the board and tables)

using namespace std;

// ---------------------------------------------------------------------
// CONSTANTS
// Fixed values used all over the program. Keeping them in one place
// makes the program easy to change (for example, the number of snakes).
// ---------------------------------------------------------------------
const int BOARD_SIZE      = 100;   // last square on the board
const int MIN_PLAYERS     = 2;
const int MAX_PLAYERS     = 4;
const int NUM_SNAKES      = 6;
const int NUM_LADDERS     = 6;
const int MAX_LEADERBOARD = 100;   // most entries kept in the file
const int MAX_NAME_LENGTH = 15;    // keeps the tables lined up
const string LEADERBOARD_FILE = "leaderboard.txt";


// =====================================================================
//                          DATA STRUCTURES
// =====================================================================

// ---------------------------------------------------------------------
// STRUCT: BoardItem
// One snake or one ladder. It only needs two numbers: the square where
// it starts and the square where it ends.
//   Snake  -> end is LOWER than start  (the player slides down)
//   Ladder -> end is HIGHER than start (the player climbs up)
// ---------------------------------------------------------------------
struct BoardItem {
    int start;
    int end;
};

// ---------------------------------------------------------------------
// DATA STRUCTURE 1: ARRAYS (snakes[] and ladders[])
// The board always has exactly 6 snakes and 6 ladders, so a fixed-size
// array is the simplest choice. They are written here in mixed order on
// purpose: main() sorts them by start square (selection sort) so that
// binary search can be used on them.
// No square is used twice, and none start on square 1 or 100.
// ---------------------------------------------------------------------
BoardItem snakes[NUM_SNAKES] = {
    {62, 19}, {17, 7}, {99, 41}, {54, 34}, {93, 73}, {87, 36}
};

BoardItem ladders[NUM_LADDERS] = {
    {28, 84}, {4, 14}, {71, 91}, {9, 31}, {40, 59}, {20, 38}
};

// ---------------------------------------------------------------------
// STRUCT: Player  (one node of the linked list)
// Holds everything we need to know about one player. The "next" pointer
// links this player to the next player in the list.
// ---------------------------------------------------------------------
struct Player {
    int id;               // player number 1-4 (shown on the board)
    string name;
    int position;         // 0 means "not on the board yet"
    int moves;            // how many turns this player has taken
    int snakesHit;
    int laddersClimbed;
    Player* next;         // pointer to the next player in the list
};

// ---------------------------------------------------------------------
// DATA STRUCTURE 2: LINKED LIST (class PlayerList)
// Stores all the players. A linked list is used because players are
// added one at a time while the game is being set up, and we can walk
// through the list to display the board, the positions and the stats.
// New players are added at the tail (end) so they stay in join order.
// ---------------------------------------------------------------------
class PlayerList {
private:
    Player* head;   // first player
    Player* tail;   // last player (lets us add at the end quickly)
    int count;      // number of players in the list

public:
    // Constructor: starts with an empty list.
    PlayerList() {
        head = NULL;
        tail = NULL;
        count = 0;
    }

    // Destructor: frees the memory of every node when the game ends.
    ~PlayerList() {
        Player* current = head;
        while (current != NULL) {
            Player* nextPlayer = current->next;
            delete current;
            current = nextPlayer;
        }
    }

    // Creates a new player node and links it at the end of the list.
    // Returns a pointer to the new player so it can be put in the queue.
    Player* addPlayer(string name) {
        Player* newPlayer = new Player;
        newPlayer->id = count + 1;
        newPlayer->name = name;
        newPlayer->position = 0;
        newPlayer->moves = 0;
        newPlayer->snakesHit = 0;
        newPlayer->laddersClimbed = 0;
        newPlayer->next = NULL;

        if (head == NULL) {          // list is empty
            head = newPlayer;
            tail = newPlayer;
        } else {                     // link after the current last node
            tail->next = newPlayer;
            tail = newPlayer;
        }
        count++;
        return newPlayer;
    }

    // LINEAR SEARCH: walks through the list one node at a time looking
    // for a name. Used to stop two players from having the same name.
    // Returns the player if found, or NULL if not found.
    Player* findPlayerByName(string name) {
        Player* current = head;
        while (current != NULL) {
            if (current->name == name) {
                return current;
            }
            current = current->next;
        }
        return NULL;
    }

    // Returns the first node so other functions can walk the list.
    Player* getHead() {
        return head;
    }

    // Returns how many players are in the list.
    int size() {
        return count;
    }
};

// ---------------------------------------------------------------------
// DATA STRUCTURE 3: QUEUE (class TurnQueue)
// Decides whose turn it is. The player at the FRONT plays. After the
// turn, that player is removed from the front (dequeue) and added to
// the back (enqueue). This gives fair round-robin order: 1,2,3,1,2,3...
// It is a circular array queue: when "rear" reaches the end of the
// array it wraps around to index 0, so no space is wasted.
// ---------------------------------------------------------------------
class TurnQueue {
private:
    Player* items[MAX_PLAYERS];   // pointers to players (not copies)
    int front;                    // index of the first item
    int rear;                     // index of the last item
    int count;                    // number of items in the queue

public:
    // Constructor: starts with an empty queue.
    TurnQueue() {
        front = 0;
        rear = -1;
        count = 0;
    }

    // Returns true if there is nobody in the queue.
    bool isEmpty() {
        return count == 0;
    }

    // Returns true if the queue cannot hold any more players.
    bool isFull() {
        return count == MAX_PLAYERS;
    }

    // Adds a player to the back of the queue.
    void enqueue(Player* player) {
        if (isFull()) {
            return;
        }
        rear = (rear + 1) % MAX_PLAYERS;   // wrap around if needed
        items[rear] = player;
        count++;
    }

    // Removes and returns the player at the front of the queue.
    Player* dequeue() {
        if (isEmpty()) {
            return NULL;
        }
        Player* player = items[front];
        front = (front + 1) % MAX_PLAYERS; // wrap around if needed
        count--;
        return player;
    }

    // Returns the player at the front WITHOUT removing them.
    // This is the player whose turn it is now.
    Player* peek() {
        if (isEmpty()) {
            return NULL;
        }
        return items[front];
    }
};

// ---------------------------------------------------------------------
// STRUCT: Move
// A record of one turn: who moved, where from, the dice value, where
// they ended up, and whether they hit a snake or climbed a ladder.
// We save all of this so that a move can be undone completely.
// ---------------------------------------------------------------------
struct Move {
    Player* player;
    int fromSquare;
    int diceValue;
    int toSquare;
    bool hitSnake;
    bool climbedLadder;
};

// ---------------------------------------------------------------------
// STRUCT: MoveNode  (one node of the stack)
// Wraps a Move together with a pointer to the node below it.
// ---------------------------------------------------------------------
struct MoveNode {
    Move data;
    MoveNode* next;
};

// ---------------------------------------------------------------------
// DATA STRUCTURE 4: STACK (class MoveStack)
// Stores the move history. A stack is "Last In, First Out" (LIFO): the
// most recent move is always on top. That is exactly what undo needs,
// because undo must reverse the LAST move first.
// It is built from linked nodes, so it can grow as long as the game
// lasts (we do not know in advance how many moves there will be).
// ---------------------------------------------------------------------
class MoveStack {
private:
    MoveNode* top;   // the most recent move
    int count;       // number of moves stored

public:
    // Constructor: starts with an empty stack.
    MoveStack() {
        top = NULL;
        count = 0;
    }

    // Destructor: removes every node to free memory.
    ~MoveStack() {
        while (!isEmpty()) {
            pop();
        }
    }

    // Returns true if there are no moves stored.
    bool isEmpty() {
        return top == NULL;
    }

    // Puts a new move on top of the stack.
    void push(Move move) {
        MoveNode* newNode = new MoveNode;
        newNode->data = move;
        newNode->next = top;   // new node sits above the old top
        top = newNode;
        count++;
    }

    // Removes the top move and returns it.
    // Always check isEmpty() before calling pop().
    Move pop() {
        MoveNode* oldTop = top;
        Move move = oldTop->data;
        top = top->next;       // the node below becomes the new top
        delete oldTop;
        count--;
        return move;
    }

    // Returns how many moves are stored.
    int size() {
        return count;
    }

    // Returns the top node so the history can be printed
    // (from the most recent move down to the oldest).
    MoveNode* getTop() {
        return top;
    }
};

// ---------------------------------------------------------------------
// STRUCT: LeaderboardEntry
// One line of the leaderboard: the winner's name and how many moves
// they needed to win. Fewer moves = better rank.
// ---------------------------------------------------------------------
struct LeaderboardEntry {
    string name;
    int moves;
};


// =====================================================================
//                     FUNCTION PROTOTYPES (overview)
// =====================================================================

// Input helpers
string trim(string text);
bool isNumber(string text);
int readInt(string prompt, int minValue, int maxValue);
string readName(string prompt);
void pressEnterToContinue();

// Sorting and searching on the snake/ladder arrays
void sortBoardItems(BoardItem items[], int size);
int binarySearch(BoardItem items[], int size, int square);
int findSnake(int square);
int findLadder(int square);

// Game actions
int rollDice();
void showDice(int value);
void printSquare(int square);
void movePlayer(Player* player, int dice, MoveStack& history);
void undoMove(MoveStack& history, TurnQueue& turns);
void giveTurnBackTo(TurnQueue& turns, Player* player);

// Display
int getSquareNumber(int row, int col);
string playersOnSquare(PlayerList& players, int square);
void displayBoard(PlayerList& players);
void showSnakesAndLadders();
void showPositions(PlayerList& players);
void showMoveHistory(MoveStack& history);
void showPlayerStats(PlayerList& players);

// Game setup and main loop
void setupPlayers(PlayerList& players, TurnQueue& turns);
void playGame();

// Leaderboard (file + bubble sort)
void loadLeaderboard(LeaderboardEntry entries[], int& count);
void bubbleSortLeaderboard(LeaderboardEntry entries[], int count);
void saveToLeaderboard(string name, int moves);
void showLeaderboard();

// Menus
void showHowToPlay();
void showMainMenu();


// =====================================================================
//                          INPUT HELPERS
// =====================================================================

// Removes spaces, tabs and '\r' from both ends of a string.
// Used so that input like "  3  " is still accepted.
string trim(string text) {
    int start = 0;
    int end = (int)text.length() - 1;
    while (start <= end && (text[start] == ' ' || text[start] == '\t' || text[start] == '\r')) {
        start++;
    }
    while (end >= start && (text[end] == ' ' || text[end] == '\t' || text[end] == '\r')) {
        end--;
    }
    return text.substr(start, end - start + 1);
}

// Returns true only if the text contains digits and nothing else.
// The length limit stops very large numbers from overflowing an int.
bool isNumber(string text) {
    if (text.length() == 0 || text.length() > 9) {
        return false;
    }
    for (int i = 0; i < (int)text.length(); i++) {
        if (text[i] < '0' || text[i] > '9') {
            return false;
        }
    }
    return true;
}

// INPUT VALIDATION: keeps asking until the user types a whole number
// between minValue and maxValue. Letters, symbols, empty input and
// out-of-range numbers are all rejected with a message.
// The whole line is read with getline so bad input never gets "stuck".
int readInt(string prompt, int minValue, int maxValue) {
    string line;
    while (true) {
        cout << prompt;
        if (!getline(cin, line)) {          // input stream closed
            cout << "\nNo more input. Goodbye!" << endl;
            exit(0);
        }
        line = trim(line);
        if (isNumber(line)) {
            int value = atoi(line.c_str());
            if (value >= minValue && value <= maxValue) {
                return value;
            }
        }
        cout << "  Invalid input! Please enter a number from "
             << minValue << " to " << maxValue << "." << endl;
    }
}

// Asks for a player name. Rejects empty names and names that are too
// long (long names would break the table layout).
string readName(string prompt) {
    string name;
    while (true) {
        cout << prompt;
        if (!getline(cin, name)) {
            cout << "\nNo more input. Goodbye!" << endl;
            exit(0);
        }
        name = trim(name);
        if (name.length() == 0) {
            cout << "  Name cannot be empty. Try again." << endl;
        } else if ((int)name.length() > MAX_NAME_LENGTH) {
            cout << "  Name is too long (max " << MAX_NAME_LENGTH << " characters)." << endl;
        } else {
            return name;
        }
    }
}

// Waits for the user to press Enter, so they can read the screen.
void pressEnterToContinue() {
    string dummy;
    cout << "\nPress Enter to continue...";
    if (!getline(cin, dummy)) {
        cout << endl;
        exit(0);
    }
}


// =====================================================================
//               SORTING AND SEARCHING (snake/ladder arrays)
// =====================================================================

// SELECTION SORT: sorts snakes or ladders by start square (smallest
// first). Each pass finds the smallest remaining item and swaps it
// into place. Binary search only works on a sorted array, so this is
// called once at the start of the program.
void sortBoardItems(BoardItem items[], int size) {
    for (int i = 0; i < size - 1; i++) {
        int minIndex = i;
        for (int j = i + 1; j < size; j++) {
            if (items[j].start < items[minIndex].start) {
                minIndex = j;
            }
        }
        if (minIndex != i) {
            BoardItem temp = items[i];
            items[i] = items[minIndex];
            items[minIndex] = temp;
        }
    }
}

// BINARY SEARCH: looks for a square in a SORTED array of snakes or
// ladders. It checks the middle item; if that is not the square, it
// throws away the half that cannot contain it and repeats.
// Returns the index of the item, or -1 if the square is not found.
// Much faster than checking every item: O(log n) instead of O(n).
int binarySearch(BoardItem items[], int size, int square) {
    int low = 0;
    int high = size - 1;
    while (low <= high) {
        int mid = (low + high) / 2;
        if (items[mid].start == square) {
            return mid;                 // found it
        } else if (items[mid].start < square) {
            low = mid + 1;              // search the right half
        } else {
            high = mid - 1;             // search the left half
        }
    }
    return -1;                          // not found
}

// Returns the index of the snake whose head is on this square, or -1.
int findSnake(int square) {
    return binarySearch(snakes, NUM_SNAKES, square);
}

// Returns the index of the ladder whose bottom is on this square, or -1.
int findLadder(int square) {
    return binarySearch(ladders, NUM_LADDERS, square);
}


// =====================================================================
//                           GAME ACTIONS
// =====================================================================

// Rolls a six-sided dice and returns a random number from 1 to 6.
int rollDice() {
    return rand() % 6 + 1;
}

// Draws the dice value in a small box so the roll is easy to see.
void showDice(int value) {
    cout << "    +-----+" << endl;
    cout << "    |  " << value << "  |" << endl;
    cout << "    +-----+" << endl;
}

// Prints a square in words. Square 0 is printed as "the start".
void printSquare(int square) {
    if (square == 0) {
        cout << "the start";
    } else {
        cout << "square " << square;
    }
}

// Moves a player by the dice value and applies the game rules:
//  - If the move would go past 100, the player stays where they are
//    (they need the exact number to land on 100).
//  - If they land on a snake head, they slide down.
//  - If they land on a ladder bottom, they climb up.
// Every move is pushed onto the history STACK so it can be undone.
void movePlayer(Player* player, int dice, MoveStack& history) {
    Move move;
    move.player = player;
    move.fromSquare = player->position;
    move.diceValue = dice;
    move.hitSnake = false;
    move.climbedLadder = false;

    int target = player->position + dice;

    if (target > BOARD_SIZE) {
        // Rule: must roll the exact number to land on 100.
        int needed = BOARD_SIZE - player->position;
        cout << "  " << player->name << " needs exactly " << needed
             << " to reach 100. Rolled " << dice << ", so "
             << player->name << " stays on square " << player->position << "." << endl;
        target = player->position;
    } else {
        cout << "  " << player->name << " moves from ";
        printSquare(player->position);
        cout << " to square " << target << "." << endl;

        // Use binary search to check for a snake or a ladder.
        int snakeIndex = findSnake(target);
        int ladderIndex = findLadder(target);

        if (snakeIndex != -1) {
            cout << "  Oh no! A SNAKE at square " << target << "! "
                 << player->name << " slides down to square "
                 << snakes[snakeIndex].end << "." << endl;
            target = snakes[snakeIndex].end;
            move.hitSnake = true;
            player->snakesHit++;
        } else if (ladderIndex != -1) {
            cout << "  Great! A LADDER at square " << target << "! "
                 << player->name << " climbs up to square "
                 << ladders[ladderIndex].end << "." << endl;
            target = ladders[ladderIndex].end;
            move.climbedLadder = true;
            player->laddersClimbed++;
        }
    }

    player->position = target;
    player->moves++;

    move.toSquare = target;
    history.push(move);       // save the move on the STACK
}

// Rotates the QUEUE until the given player is at the front.
// Taking a player off the front and adding them to the back does not
// change the round-robin order; it only changes who goes first.
// Used by undo to give the turn back to the player whose move was undone.
void giveTurnBackTo(TurnQueue& turns, Player* player) {
    while (turns.peek() != player) {
        turns.enqueue(turns.dequeue());
    }
}

// UNDO: pops the last move off the STACK and reverses it:
//  - puts the player back on the square they started from,
//  - takes away the move, snake or ladder that was counted,
//  - gives the turn back to that player (using the QUEUE).
// Undo can be used again and again, all the way back to the start.
void undoMove(MoveStack& history, TurnQueue& turns) {
    if (history.isEmpty()) {
        cout << "  Nothing to undo. No moves have been made yet." << endl;
        return;
    }

    Move lastMove = history.pop();
    Player* player = lastMove.player;

    player->position = lastMove.fromSquare;
    player->moves--;
    if (lastMove.hitSnake) {
        player->snakesHit--;
    }
    if (lastMove.climbedLadder) {
        player->laddersClimbed--;
    }

    giveTurnBackTo(turns, player);

    cout << "  UNDO: " << player->name << " goes back from square "
         << lastMove.toSquare << " to ";
    printSquare(lastMove.fromSquare);
    cout << "." << endl;
    cout << "  It is " << player->name << "'s turn again." << endl;
}


// =====================================================================
//                              DISPLAY
// =====================================================================

// Converts a board row and column into a square number.
// Row 0 is the bottom row (squares 1-10) and row 9 is the top row
// (squares 91-100). Rows go in a zig-zag: even rows run left to right,
// odd rows run right to left, just like a real board.
int getSquareNumber(int row, int col) {
    if (row % 2 == 0) {
        return row * 10 + col + 1;      // left to right
    } else {
        return row * 10 + (10 - col);   // right to left
    }
}

// Walks through the player LINKED LIST and returns the numbers of all
// players standing on this square, for example "13" for players 1 and 3.
string playersOnSquare(PlayerList& players, int square) {
    string result = "";
    Player* current = players.getHead();
    while (current != NULL) {
        if (current->position == square) {
            result += (char)('0' + current->id);
        }
        current = current->next;
    }
    return result;
}

// Prints the 10x10 board with square 100 in the top-left corner.
// Each cell shows: the square number, S (snake head) or L (ladder
// bottom), and the numbers of any players on that square.
void displayBoard(PlayerList& players) {
    string line = "+";
    for (int i = 0; i < 10; i++) {
        line += "--------+";
    }

    cout << endl << line << endl;
    for (int row = 9; row >= 0; row--) {
        cout << "|";
        for (int col = 0; col < 10; col++) {
            int square = getSquareNumber(row, col);

            cout << setw(3) << square;

            if (findSnake(square) != -1) {
                cout << "S";
            } else if (findLadder(square) != -1) {
                cout << "L";
            } else {
                cout << " ";
            }

            cout << left << setw(4) << playersOnSquare(players, square) << right << "|";
        }
        cout << endl << line << endl;
    }

    cout << "Key: S = snake head, L = ladder bottom, 1-4 = player numbers" << endl;

    // Players still at the start (square 0) are not on the grid.
    string waiting = playersOnSquare(players, 0);
    if (waiting != "") {
        cout << "Still at the start: player(s) " << waiting << endl;
    }
    showSnakesAndLadders();
}

// Prints every snake and ladder from the arrays (already sorted).
void showSnakesAndLadders() {
    cout << "Snakes : ";
    for (int i = 0; i < NUM_SNAKES; i++) {
        cout << snakes[i].start << "->" << snakes[i].end << "  ";
    }
    cout << endl << "Ladders: ";
    for (int i = 0; i < NUM_LADDERS; i++) {
        cout << ladders[i].start << "->" << ladders[i].end << "  ";
    }
    cout << endl;
}

// Prints one short line with every player's current square
// by walking through the LINKED LIST.
void showPositions(PlayerList& players) {
    cout << "  Positions: ";
    Player* current = players.getHead();
    while (current != NULL) {
        cout << current->name << "(P" << current->id << ")=" << current->position;
        if (current->next != NULL) {
            cout << ", ";
        }
        current = current->next;
    }
    cout << endl;
}

// Prints the move history from the STACK, newest move first
// (top of the stack is the most recent move).
void showMoveHistory(MoveStack& history) {
    cout << endl << "===== MOVE HISTORY (newest first) =====" << endl;
    if (history.isEmpty()) {
        cout << "  No moves yet." << endl;
        return;
    }

    int moveNumber = history.size();
    MoveNode* current = history.getTop();
    while (current != NULL) {
        Move m = current->data;
        cout << "  #" << setw(3) << left << moveNumber << right
             << setw(MAX_NAME_LENGTH) << left << m.player->name << right
             << " rolled " << m.diceValue << ": " << setw(3) << m.fromSquare
             << " -> " << setw(3) << m.toSquare;
        if (m.hitSnake) {
            cout << "  (snake)";
        } else if (m.climbedLadder) {
            cout << "  (ladder)";
        } else if (m.fromSquare == m.toSquare) {
            cout << "  (needed exact roll)";
        }
        cout << endl;
        moveNumber--;
        current = current->next;
    }
}

// Prints a table of every player's stats by walking the LINKED LIST:
// current square, total moves, snakes hit and ladders climbed.
void showPlayerStats(PlayerList& players) {
    cout << endl << "===== PLAYER STATS =====" << endl;
    cout << left << setw(4) << "No"
         << setw(MAX_NAME_LENGTH + 2) << "Name"
         << setw(8) << "Square"
         << setw(8) << "Moves"
         << setw(8) << "Snakes"
         << setw(8) << "Ladders" << right << endl;
    cout << "-----------------------------------------------------" << endl;

    Player* current = players.getHead();
    while (current != NULL) {
        cout << left << setw(4) << current->id
             << setw(MAX_NAME_LENGTH + 2) << current->name
             << setw(8) << current->position
             << setw(8) << current->moves
             << setw(8) << current->snakesHit
             << setw(8) << current->laddersClimbed << right << endl;
        current = current->next;
    }
}


// =====================================================================
//                     GAME SETUP AND MAIN GAME LOOP
// =====================================================================

// Asks how many players there are (2-4) and their names.
// Each player is added to the LINKED LIST, then to the turn QUEUE.
// A LINEAR SEARCH on the list makes sure every name is different.
void setupPlayers(PlayerList& players, TurnQueue& turns) {
    int numPlayers = readInt("Enter number of players (2-4): ", MIN_PLAYERS, MAX_PLAYERS);

    for (int i = 1; i <= numPlayers; i++) {
        // Build the prompt text, for example "Player 2 - enter name: "
        string prompt = "Player ";
        prompt += (char)('0' + i);
        prompt += " - enter name: ";

        string name;
        while (true) {
            name = readName(prompt);
            if (players.findPlayerByName(name) == NULL) {
                break;                      // name is free
            }
            cout << "  That name is already taken. Choose another." << endl;
        }
        Player* newPlayer = players.addPlayer(name);
        turns.enqueue(newPlayer);
    }
}

// Runs one full game: setup, then turns until someone reaches 100 or
// the players quit. The player at the front of the QUEUE always plays.
void playGame() {
    PlayerList players;    // LINKED LIST of players
    TurnQueue turns;       // QUEUE of turns
    MoveStack history;     // STACK of moves

    cout << endl << "========== NEW GAME ==========" << endl;
    setupPlayers(players, turns);

    Player* winner = NULL;
    bool quit = false;

    while (winner == NULL && !quit) {
        Player* current = turns.peek();

        cout << endl << "------------------------------------------" << endl;
        cout << "  TURN: " << current->name << " (Player " << current->id
             << ")  -  on ";
        printSquare(current->position);
        cout << endl;
        showPositions(players);
        cout << "------------------------------------------" << endl;
        cout << "  1. Roll dice" << endl;
        cout << "  2. Show board" << endl;
        cout << "  3. Show move history" << endl;
        cout << "  4. Undo last move" << endl;
        cout << "  5. Show player stats" << endl;
        cout << "  6. Quit to main menu" << endl;

        int choice = readInt("Choose (1-6): ", 1, 6);

        if (choice == 1) {
            int dice = rollDice();
            cout << endl << "  " << current->name << " rolls the dice..." << endl;
            showDice(dice);
            movePlayer(current, dice, history);

            if (current->position == BOARD_SIZE) {
                winner = current;
            } else {
                // Turn is over: move this player from the front to the back.
                turns.dequeue();
                turns.enqueue(current);
            }
        } else if (choice == 2) {
            displayBoard(players);
        } else if (choice == 3) {
            showMoveHistory(history);
        } else if (choice == 4) {
            undoMove(history, turns);
        } else if (choice == 5) {
            showPlayerStats(players);
        } else if (choice == 6) {
            int sure = readInt("Are you sure you want to quit this game? (1 = Yes, 2 = No): ", 1, 2);
            if (sure == 1) {
                quit = true;
            }
        }
    }

    if (winner != NULL) {
        cout << endl << "******************************************" << endl;
        cout << "  " << winner->name << " reached square 100 and WINS in "
             << winner->moves << " moves!" << endl;
        cout << "******************************************" << endl;
        displayBoard(players);
        showPlayerStats(players);
        saveToLeaderboard(winner->name, winner->moves);
        cout << endl << "  " << winner->name << " has been added to the leaderboard." << endl;
    } else {
        cout << endl << "  Game ended with no winner." << endl;
        showPlayerStats(players);
    }
    pressEnterToContinue();
}


// =====================================================================
//                    LEADERBOARD (file + bubble sort)
// =====================================================================

// Reads the leaderboard file into an array. Each line of the file is:
//   <moves> <name>
// The number comes first so that names with spaces still work.
// If the file does not exist yet, the leaderboard is simply empty.
void loadLeaderboard(LeaderboardEntry entries[], int& count) {
    count = 0;
    ifstream file(LEADERBOARD_FILE.c_str());
    if (!file) {
        return;
    }

    int moves;
    string name;
    while (count < MAX_LEADERBOARD && file >> moves) {
        getline(file, name);          // rest of the line is the name
        name = trim(name);
        if (name != "" && moves > 0) {
            entries[count].name = name;
            entries[count].moves = moves;
            count++;
        }
    }
    file.close();
}

// BUBBLE SORT: orders the leaderboard by moves, fewest moves first.
// It compares each pair of neighbours and swaps them if they are in the
// wrong order. After each pass the largest value "bubbles" to the end.
// If a whole pass makes no swaps, the array is sorted and we stop early.
void bubbleSortLeaderboard(LeaderboardEntry entries[], int count) {
    for (int i = 0; i < count - 1; i++) {
        bool swapped = false;
        for (int j = 0; j < count - 1 - i; j++) {
            if (entries[j].moves > entries[j + 1].moves) {
                LeaderboardEntry temp = entries[j];
                entries[j] = entries[j + 1];
                entries[j + 1] = temp;
                swapped = true;
            }
        }
        if (!swapped) {
            break;
        }
    }
}

// Adds a winner to the leaderboard file. It loads the old entries,
// adds the new one, sorts them with bubble sort and writes them all
// back, so the file is always sorted and never grows past 100 lines.
void saveToLeaderboard(string name, int moves) {
    LeaderboardEntry entries[MAX_LEADERBOARD];
    int count = 0;
    loadLeaderboard(entries, count);
    bubbleSortLeaderboard(entries, count);

    if (count < MAX_LEADERBOARD) {
        entries[count].name = name;
        entries[count].moves = moves;
        count++;
    } else if (moves < entries[count - 1].moves) {
        // Leaderboard is full: replace the worst score if the new one is better.
        entries[count - 1].name = name;
        entries[count - 1].moves = moves;
    }
    bubbleSortLeaderboard(entries, count);

    ofstream file(LEADERBOARD_FILE.c_str());
    if (!file) {
        cout << "  Error: could not save the leaderboard file." << endl;
        return;
    }
    for (int i = 0; i < count; i++) {
        file << entries[i].moves << " " << entries[i].name << endl;
    }
    file.close();
}

// Loads the leaderboard from the file, sorts it with bubble sort and
// prints the top 10 winners (fewest moves first).
void showLeaderboard() {
    LeaderboardEntry entries[MAX_LEADERBOARD];
    int count = 0;
    loadLeaderboard(entries, count);
    bubbleSortLeaderboard(entries, count);

    cout << endl << "========== LEADERBOARD (fewest moves wins) ==========" << endl;
    if (count == 0) {
        cout << "  No games have been won yet. Play a game first!" << endl;
    } else {
        cout << left << setw(6) << "Rank" << setw(MAX_NAME_LENGTH + 2) << "Name"
             << "Moves" << right << endl;
        cout << "------------------------------" << endl;
        int shown = (count < 10) ? count : 10;
        for (int i = 0; i < shown; i++) {
            cout << left << setw(6) << (i + 1)
                 << setw(MAX_NAME_LENGTH + 2) << entries[i].name
                 << entries[i].moves << right << endl;
        }
    }
    pressEnterToContinue();
}


// =====================================================================
//                               MENUS
// =====================================================================

// Prints the rules of the game and explains the undo option.
void showHowToPlay() {
    cout << endl << "=================== HOW TO PLAY ===================" << endl;
    cout << " 1. 2 to 4 players take turns in a fixed order." << endl;
    cout << " 2. Everyone starts off the board (square 0)." << endl;
    cout << " 3. On your turn, roll the dice (1 to 6) and move forward." << endl;
    cout << " 4. Land on a SNAKE head (S) and you slide down to its tail." << endl;
    cout << " 5. Land on a LADDER bottom (L) and you climb up to its top." << endl;
    cout << " 6. You must roll the EXACT number to land on square 100." << endl;
    cout << "    If the roll is too big, you stay where you are." << endl;
    cout << " 7. The first player to reach square 100 wins!" << endl;
    cout << endl;
    cout << " UNDO: choose 'Undo last move' to cancel the most recent" << endl;
    cout << " move. That player goes back to where they were, and it" << endl;
    cout << " becomes their turn again. You can undo many times." << endl;
    cout << endl;
    cout << " During a turn you can also view the board, the move" << endl;
    cout << " history and every player's stats." << endl;
    cout << " The winner is saved to the leaderboard (fewest moves = best)." << endl;
    cout << endl;
    showSnakesAndLadders();
    cout << "===================================================" << endl;
    pressEnterToContinue();
}

// Prints the main menu options.
void showMainMenu() {
    cout << endl;
    cout << "=====================================" << endl;
    cout << "         SNAKES  AND  LADDERS        " << endl;
    cout << "=====================================" << endl;
    cout << "  1. Play" << endl;
    cout << "  2. How to Play" << endl;
    cout << "  3. View Leaderboard" << endl;
    cout << "  4. Exit" << endl;
    cout << "=====================================" << endl;
}

// Program starts here. Seeds the dice, sorts the snake and ladder
// arrays (so binary search works), then shows the main menu until the
// user chooses Exit.
int main() {
    srand((unsigned int)time(NULL));

    sortBoardItems(snakes, NUM_SNAKES);
    sortBoardItems(ladders, NUM_LADDERS);

    int choice = 0;
    while (choice != 4) {
        showMainMenu();
        choice = readInt("Enter your choice (1-4): ", 1, 4);

        if (choice == 1) {
            playGame();
        } else if (choice == 2) {
            showHowTo2Play();
        } else if (choice == 3) {
            showLeaderboard();
        }
    }

    cout << "Thanks for playing! Goodbye." << endl;
    return 0;
}
