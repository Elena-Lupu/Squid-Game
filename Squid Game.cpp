#include <iostream>
#include <xdevapi.h> //Used for the connection to the database
#include <vector>
#include <algorithm>
#include <random>
#include <stdlib.h>
#include <time.h>

using namespace mysqlx;
using namespace std;

class Game
{
    private:
        //This is the prize for the winner
        static unsigned int piggy_bank; 

    public:
        virtual void play(Session s) = 0;

        static void add_money(unsigned int s) { piggy_bank += s;}
        static unsigned int get_money() { return piggy_bank; }
};

unsigned int Game::piggy_bank = 0;

class User
{
    protected:
        std::string last_name, first_name, city;
        unsigned int id, weight, debt;

        User(Row r)
        {
            id = (unsigned int)r.get(0);
            last_name = (std::string)r.get(1);
            first_name = (std::string)r.get(2);
            city = (std::string)r.get(3);
            weight = (unsigned int)r.get(4);
            debt = (unsigned int)r.get(5);
        }
    
    public:
        User()
        {
            last_name = first_name = city = "default";
            id = weight = debt = 0;
        }
};

template <class T>
void insertion_sort(vector<T> &v,unsigned int nr)
{
    unsigned int index=0,index2=0,index3=0;
    T aux;

    for (index = 1 ; index < nr ; index++)
        if (v[index] < v[index-1])
        {
            aux = v[index];
            for (index2 = 0 ; index2 < index ; index2++)
                if (v[index2] > v[index])
                {
                    for (index3 = index ; index3 > index2 ; index3--) v[index3] = v[index3-1];
                    v[index2] = aux;
                    break;
                }
        }
}

class Player: public User
{
    private:
        unsigned int id_supervisor, player_nr;

        Player(Row r): User{r}
        {
            id_supervisor = (unsigned int)r.get(6);
            player_nr = (unsigned int)r.get(7);
        }

    public:
        Player(): User{} { id_supervisor = player_nr = 0; }
        ~Player() { cout<<"-"; } //for each deleted Player object, it displays a '-', so after each game there will be a line...

        bool operator >(const Player& p) { return this->last_name>p.last_name; }
        bool operator <(const Player& p) { return this->last_name<p.last_name; }

        static void erase_player(SqlResult *sr, Session *s)
        {
            Row line;
            std::string str;
            unsigned int deb;

            while (sr->hasData())
            {
                line = sr->fetchOne();
                deb = (unsigned int)line.get(1);
                str = "UPDATE supervisors SET sum=sum+" + to_string(deb) + " WHERE supervisor_nr=" + to_string((unsigned int)line.get(2)) + ";"; s->sql(str).execute();
                Game::add_money(deb);
                str = "DELETE FROM players WHERE id=" + to_string((unsigned int)line.get(0)) + ";"; s->sql(str).execute();
            }
        }

        static void erase_player(vector<unsigned int> v, Session *s)
        {
            unsigned short stop=v.size();
            unsigned int deb;
            std::string str;
            Row line;
            //For each id in vector, take the matching line from the database and add the debt to piggy_bank and to the coresponding supervisor's sum field, then erase the line
            for (unsigned short i = 0 ; i < stop ; i++)
            {
                str = "SELECT debt,id_supervisor FROM players WHERE id=" + to_string(v[i]) + ";";
                line = (s->sql(str).execute()).fetchOne();
                deb = (unsigned int)line.get(0);
                str = "UPDATE supervisors SET sum=sum+" + to_string(deb) + " WHERE supervisor_nr=" + to_string((unsigned int)line.get(1)) + ";"; s->sql(str).execute();
                Game::add_money(deb);
                str = "DELETE FROM players WHERE id=" + to_string(v[i]) + ";"; s->sql(str).execute();
            }
        }

        static void print(SqlResult *sr)
        {
            vector<Player> list;
            Row line;
            unsigned short sh;
            //Create a vector of players, sort it after last_name and display it
            while (sr->hasData())
            {
                line = sr->fetchOne();
                list.push_back(Player(line));
            }
            sh = list.size();
            insertion_sort<Player>(list, sh);
            cout<<endl;
            for (unsigned short i = 0 ; i < sh ; i++) cout<<list[i].player_nr<<"     "<<list[i].last_name<<"   "<<list[i].first_name<<endl;
            list.clear();
        }
};

class Supervisor: public User
{
    private:
        std::string mask_form;
        unsigned int sum=0, supervisor_nr;

        Supervisor(Row r): User{r}
        {
            supervisor_nr = (unsigned int)r.get(8);
            sum = (unsigned int)r.get(7);
            mask_form = (std::string)r.get(6);
        }
    
    public:
        bool operator >(const Supervisor& p) { return this->sum<p.sum; }
        bool operator <(const Supervisor& p) { return this->sum>p.sum; }

        Supervisor(): User{}
        {
            supervisor_nr = sum = 0;
            mask_form = "default";
        }

        static void print(SqlResult *sr)
        {
            vector<Supervisor> list;
            Row line;
            unsigned short sh;
            //Create a vector of supervisors, sort it after the sum and display it
            while (sr->hasData())
            {
                line = sr->fetchOne();
                list.push_back(Supervisor(line));
            }
            sh = list.size();
            insertion_sort<Supervisor>(list, sh);
            for (unsigned short i = 0 ; i < sh ; i++) cout<<list[i].last_name<<"   "<<list[i].first_name<<"     "<<list[i].sum<<endl;
            list.clear();
        }

        static unsigned int get_total(SqlResult *sr, std::string shape)
        {
            Row line;
            unsigned int s=0;
            //Gets the total of sum fields of the supervisors with the mask_form='shape'
            while (sr->hasData())
            {
                line = sr->fetchOne();
                if ((std::string)line.get(1) == shape) s += (unsigned int)line.get(0);
            }

            return s;
        }
};

class RedLightGreenLight: public Game
{
    public:
        static void play(Session *s)
        {
            SqlResult *droped = new SqlResult;
            //Select the players matching the condition and send the result to the erasing function
            *droped = s->sql("SELECT id, debt, id_supervisor FROM players WHERE player_nr%2=0;").execute();
            Player::erase_player(droped, s);
            *droped = s->sql("SELECT * FROM players;").execute();
            cout<<"\nWinners are:\n\n";
            Player::print(droped);
        }
};

class TugOfWar: public Game
{
    private:
        static unsigned int get_weight(vector<unsigned int> v, unsigned short team_nr, Session *s)
        {
            unsigned int w=0;
            Row line;
            std::string str;
            //Select the weights from the database where the id mathces and add them to w
            for (unsigned short i = (team_nr-1)*12 ; i < 12*team_nr ; i++)
            {
                str = "SELECT weight FROM players WHERE id=" + to_string(v[i]) + ";";
                line = (s->sql(str).execute()).fetchOne();
                w += (unsigned int)line.get(0);
            }

            return w;
        }

    public:
        static void play(Session *s)
        {
            vector<unsigned int> ids, drop_ids;
            unsigned int total_weight1, total_weight2;
            SqlResult *sr = new SqlResult;
            //Take the ids and shuffle them, then calculate the first 2 teams weights; ids of the lighter team are put in a vector for eliminating
            *sr = s->sql("SELECT id FROM players;").execute();
            while (sr->hasData()) ids.push_back((unsigned int)(sr->fetchOne()).get(0));
            shuffle(ids.begin(), ids.end(), default_random_engine());

            total_weight1 = TugOfWar::get_weight(ids, 1, s);
            total_weight2 = TugOfWar::get_weight(ids, 2, s);
            if (total_weight1 > total_weight2) for (unsigned short i = 12 ; i < 24 ; i++) drop_ids.push_back(ids[i]);
            else for (unsigned short i = 0 ; i < 12 ; i++) drop_ids.push_back(ids[i]);
            //Overwrite total_weight1 and total_weight2 with the weights of the next 2 teams
            total_weight1 = TugOfWar::get_weight(ids, 3, s);
            total_weight2 = TugOfWar::get_weight(ids, 4, s);
            if (total_weight1 > total_weight2) for (unsigned short i = 36 ; i < 48 ; i++) drop_ids.push_back(ids[i]);
            else for (unsigned short i = 24 ; i < 36 ; i++) drop_ids.push_back(ids[i]);

            Player::erase_player(drop_ids, s);
            *sr = s->sql("SELECT * FROM players;").execute();
            cout<<"\nWinners are:\n\n";
            Player::print(sr);

            ids.clear(); drop_ids.clear();
        }
};

class Marbles: public Game
{
    public:
        static void play(Session *s)
        {
            vector<unsigned int> ids, drop_ids;
            SqlResult *sr = new SqlResult;
            unsigned int nr_1, nr_2;
            //Take the ids and sguffle them, then for each 2 players, select 2 random numbers
            srand (time(NULL));
            *sr = s->sql("SELECT id FROM players;").execute();
            while (sr->hasData()) ids.push_back((unsigned int)(sr->fetchOne()).get(0));
            shuffle(ids.begin(), ids.end(), default_random_engine());

            for (unsigned short i = 0 ; i < 26 ; i = i+2)
            {
                nr_1 = rand() % 100 + 1; nr_2 = rand() % 100 + 1;
                if (nr_1 > nr_2) drop_ids.push_back(ids[i]);
                else drop_ids.push_back(ids[i+1]);
            }

            Player::erase_player(drop_ids, s);
            *sr = s->sql("SELECT * FROM players;").execute();
            cout<<"\nWinners are:\n\n";
            Player::print(sr);

            ids.clear(); drop_ids.clear();
        }
};

class Genken: public Game
{
    public:
        static void play(Session *s)
        {
            vector<unsigned int> ids, drop_ids;
            SqlResult *sr = new SqlResult;
            unsigned int nr_1, nr_2, stop, is, count;
            srand (time(NULL));
            std::string str;
            //While there are more than 1 players left, repeat the sequence: peak the ids, shuffle them and peak each 2 players and generate 2 random numbers
            COUNT:
            count = (unsigned int)((s->sql("SELECT COUNT(id) FROM players;").execute()).fetchOne().get(0)); 

            if (count > 1)
            {
                *sr = s->sql("SELECT id FROM players;").execute();
                while (sr->hasData()) ids.push_back((unsigned int)(sr->fetchOne()).get(0));
                shuffle(ids.begin(), ids.end(), default_random_engine());
                stop = ids.size();

                for (unsigned short i = 0 ; i < stop-1 ; i = i+2)
                {
                    do { nr_1 = rand()%3+1; nr_2 = rand()%3+1; }while(nr_1 == nr_2);
                    if ((nr_1 == 1 && nr_2 == 2) || (nr_1 == 2 && nr_2 == 3) || (nr_1 == 3 && nr_2 == 1)) drop_ids.push_back(ids[i]);
                    if ((nr_1 == 2 && nr_2 == 1) || (nr_1 == 3 && nr_2 == 2) || (nr_1 == 1 && nr_2 == 3)) drop_ids.push_back(ids[i+1]);
                }
                Player::erase_player(drop_ids, s);
                ids.clear(); drop_ids.clear();

                goto COUNT;
            }
            //Add the winner's supervisor's debt*9 to its sum; it is *9 because I've assumed that the supervisors' initial debts are not put into sum
            *sr = s->sql("SELECT id_supervisor FROM players;").execute();
            is = (unsigned int)(sr->fetchOne()).get(0);
            str = "UPDATE supervisors SET sum=sum+9*debt WHERE supervisor_nr=" + to_string(is) + ";"; s->sql(str).execute();
            cout<<"\nThe final winner is...:\n\n";
            *sr = s->sql("SELECT * FROM players;").execute();
            Player::print(sr);

            ids.clear(); drop_ids.clear();
        }
};

int main()
{
    vector<unsigned short> ids;
    std::string str;
    SqlResult *sr = new SqlResult;
    unsigned int r,t,c;

    try
    {
        //Create a new connection to the database
        Session *ses = new Session("mysqlx://root:PufuleatzaZzDuP27!*@localhost:33060");
        if (!ses) throw "Cannot connect to database !";
        //Using the stored procedure "create_users" to fill the "users" table
        ses->sql("USE squid_game;").execute();
        ses->sql("CALL create_users;").execute();
        //save the users ids in a vector and shuffle them, to random select the supervisors and the players
        for (unsigned short i = 1 ; i <= 108 ; i++) ids.push_back(i);
        shuffle(ids.begin(), ids.end(), default_random_engine());
        for (unsigned short i = 0 ; i <= 8 ; i++)
        {
            str = "INSERT INTO supervisors(id, last_name, first_name, city, weight, debt) SELECT * FROM users WHERE id=" + to_string(ids[i]) + ";"; ses->sql(str).execute();
            if (i+1 == 1 || i+1 == 2 || i+1 == 3)
            { str = "UPDATE supervisors SET mask_form=\"Triangle\" WHERE id=" + to_string(ids[i]) + ";"; ses->sql(str).execute(); }
            if (i+1 == 4 || i+1 == 5 || i+1 == 6)
            { str = "UPDATE supervisors SET mask_form=\"Rectangle\" WHERE id=" + to_string(ids[i]) + ";"; ses->sql(str).execute(); }
            if (i+1 == 7 || i+1 == 8 || i+1 == 9)
            { str = "UPDATE supervisors SET mask_form=\"Circle\" WHERE id=" + to_string(ids[i]) + ";"; ses->sql(str).execute(); }
        }
        for (unsigned short i = 9; i <= 107 ; i++)
        {
            str = "INSERT INTO players(id, last_name, first_name, city, weight, debt) SELECT * FROM users WHERE id=" + to_string(ids[i]) + ";"; ses->sql(str).execute();
            str = "UPDATE players SET player_nr=" + to_string(i+1) + "-9 WHERE id=" + to_string(ids[i]) + ";"; ses->sql(str).execute();
            str = "UPDATE players SET id_supervisor=CEILING((" + to_string(i+1) + "-9)/11) WHERE id=" + to_string(ids[i]) + ";"; ses->sql(str).execute();
        }
        ids.clear();

        cout<<"\n\n---------------------------------\n\nLet the Squid Game begin !\n\n------------------------\n\n";
    
        cout<<"\n1. Red Light Green Light:\n";
        RedLightGreenLight::play(ses);

        cout<<"\n2. Tug of War:\n";
        TugOfWar::play(ses);

        cout<<"\n3. Marbles:\n";
        Marbles::play(ses);

        cout<<"\n4. Genken:\n";
        Genken::play(ses);

        cout<<"The winner has won:   "<<Game::get_money();

        cout<<"\n\n----------------------\n\nAnd now, the supervisors with the most money earned are:\n\n";
        *sr = ses->sql("SELECT * FROM supervisors;").execute();
        Supervisor::print(sr);
        *sr = ses->sql("SELECT sum, mask_form FROM supervisors;").execute();
        r = Supervisor::get_total(sr, "Rectangle");
        t = Supervisor::get_total(sr, "Triangle");
        c = Supervisor::get_total(sr, "Circle");
        cout<<"\n\nThe winning supervisor team is:   ";
        if (r > t && r > c) cout<<"Rectangle";
        if (t > r && t > c) cout<<"Triangle";
        if (c > t && c > r) cout<<"Circle";

        cout<<"\n\n-------------------------------------\n\nThanks for playing !\n\n";

        ses->close();
    } catch (const char* er) { cout<<endl<<er<<endl; exit(1); }

    return 0;
}