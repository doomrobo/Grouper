/*
 * Licensed under the terms of the zlib license
 * http://www.gzip.org/zlib/zlib_license.html
 *
*/



#include <iostream>
#include <fstream>
#include <vector>
#include <list>
#include <map>
#include <boost/bimap.hpp>
#include <boost/bimap/unordered_set_of.hpp>
#include <cstdlib>

using namespace std;

#ifndef _DEBUG
#define DEBUG if(false)
#else
#define DEBUG if(true)
#endif

struct combination {
	int *comb_seq;
	bool dead;
	combination(): dead(false) {comb_seq = NULL;}
	~combination() {if (comb_seq != NULL) delete[] comb_seq;}
};

struct choice {
	int chooser;
	size_t num_choices;
	int *choices;
};

typedef boost::bimap<
	boost::bimaps::unordered_set_of<string>,
	boost::bimaps::unordered_set_of<size_t>
	> string_int_bimap;

typedef string_int_bimap::value_type string_int_val;

bool next_combination(size_t seq_len, size_t choose, struct combination *last_comb) {
	if (last_comb->dead || choose > seq_len || choose == 0)
		return false;

	if (last_comb->comb_seq == NULL) {
		last_comb->comb_seq = new int[choose];
		for (size_t i = 0; i < choose; i++) {
			last_comb->comb_seq[i] = i;
		}
		if (choose == seq_len) // only one combination
			last_comb->dead = true;
	}
	else {
		for (int i = choose-1; i >= 0; i--) {
			if (last_comb->comb_seq[i]+1+(choose-1-i) < seq_len) { // We can increase this element
				last_comb->comb_seq[i]++;
				for (size_t j = 1; j < choose-i; j++)
					last_comb->comb_seq[i+j] = last_comb->comb_seq[i]+j; // Reset the remaining elements to the right
				return true;
			}
		}
		return false;
	}
	return true;
}


string_int_bimap map_names_to_ids(string filename) {
	string_int_bimap name_bimap;
	ifstream file(filename.c_str());
	string line;
	size_t counter = 0;
	getline(file, line); // Skip the first line, it contains data for later
	while (getline(file, line)) {
		if (line.length() > 0 && line[0] != '\t' && line[0] != ' ') { // We have a new person with choices
			name_bimap.insert(string_int_val(line.c_str(), counter));
			counter++;
		}
	}
	file.close();

	return name_bimap;
}

inline int find_first_non_white(string &str) {
	for (size_t i = 0; i < str.length(); i++) {
		if (str[i] != ' ' && str[i] != '\t')
			return i;
	}
	return -1;
}


vector<choice> parse_groups(string filename, string_int_bimap &lookup, size_t &group_size) {
	vector<choice> choices;
	string line;
	size_t choice_size, running_choices = 0;
	ifstream file_in(filename.c_str(), fstream::in);
	file_in >> group_size; // First line is an integer with how big the groups are
	choice_size = group_size-1;
	getline(file_in, line);
	choice curr_choice;
	bool started = false;
	int end_of_whitespace;
	while (getline(file_in, line)) {
		if (line.length() == 0)
			continue;
		else if (line[0] == '\t' || line[0] == ' ') {
			end_of_whitespace = find_first_non_white(line);
			DEBUG
				cerr << "end_of_whitespace on \"" << line << "\" is " << end_of_whitespace << endl;
			if (end_of_whitespace == -1)
				continue;
			curr_choice.choices[running_choices] = lookup.left.at(line.substr(end_of_whitespace,line.length()-1).c_str());
			running_choices++;
		} else {
			if (started) {
				if (running_choices < choice_size) {
					DEBUG
						cerr << "Filling remaining choices for " << lookup.right.at(curr_choice.chooser) << endl;
					for (size_t i = running_choices; i < choice_size; i++) {
						curr_choice.choices[i] = -1;
					}
				}
				choices.push_back(curr_choice);
				running_choices = 0;
			} else
				started = true;

			curr_choice.choices = new int[choice_size];
			curr_choice.num_choices = choice_size;
			curr_choice.chooser = lookup.left.at(line.c_str());
		}
	}
	choices.push_back(curr_choice);

	file_in.close();
	return choices;
}

bool chosen(choice &c, int check) {
	for (size_t i = 0; i < c.num_choices; i++) {
		if (c.choices[i] == check)
			return true;
	}
	return false;
}

void nullify_duplicates(vector<choice> &choices) {
	for (auto &c : choices) { // duplicate comparison in O(n choose 2) time :-(
		for (size_t i = 0; i < c.num_choices-1; i++) {
			for (size_t j = i+1; j < c.num_choices; j++) {
				if (c.choices[i] == -1 || c.choices[j] == -1)
					continue;
				if (c.choices[i] == c.choices[j]) {
					DEBUG
						cerr << c.chooser << " chose a duplicate of " << c.choices[i] << endl;
					c.choices[j] = -1; // nullify choice
				}
			}
		}
	}
}

size_t evaluate_combination(size_t group_size, vector<choice> &groups, combination &c) {
	unsigned score = 0;

	for (size_t i = 0; i < group_size; i++) {
		choice me = groups[c.comb_seq[i]];
		for (size_t j = i+1; j < group_size; j++) {
			choice other = groups[c.comb_seq[j]];
			if (other.chooser < me.chooser) // We already evaluated this pairing
				continue;
			if (chosen(me, other.chooser)) { // if I chose the other guy
				if (chosen(other, me.chooser)) { // if the other guy chose me
					score += 3;
				}
				else
					score += 1;
			}
		}
	}
	return score;
}

bool check_add_group(list<int> &group, list<list<int> > &final_groups, bool* &person_taken) {
	for (int x : group)
		if (person_taken[x])
			return false;
	// if it makes it here that means no members are conflicting with other groups
	final_groups.push_back(group);
	for (int x : group)
		person_taken[x] = true;

	return true;
}

int main(int argc, char **argv) {
	srand(time(NULL));
	string filename;
	size_t group_size;
	if (argc > 1)
		filename.assign(argv[1]);
	else {
		cerr << "No input file!" << endl;
		return 1;
	}

	string_int_bimap name_bimap;
	name_bimap = map_names_to_ids(filename);
	vector<choice> gs = parse_groups(filename, name_bimap, group_size);
	nullify_duplicates(gs);
	DEBUG {
		for (auto c : gs) {
			cerr << c.chooser << " chose ";
			for (size_t i = 0; i < c.num_choices; i++) {
				cerr << c.choices[i] << ", ";
			}
			cerr << endl;
		}
	}

	map<size_t, vector<list<int> > > group_scores; // Mapping group fit scores to list of members
	set<size_t> scores; // List of the unique scores, used for iterating
	bool *person_taken = new bool[name_bimap.size()]();
	list<list<int> > final_groups;

	struct combination comb;
	const int CHOOSE = group_size;
	const int SEQ_LEN = gs.size();
	size_t score;
	while (next_combination(SEQ_LEN, CHOOSE, &comb)) {
		score = evaluate_combination(CHOOSE, gs, comb);
		list<int> tmp_list;
		for (size_t i = 0; i < group_size; i++) {
			tmp_list.push_back(comb.comb_seq[i]);
		}
		DEBUG {
			cerr << "(";
			for (auto i = tmp_list.begin(); i != tmp_list.end(); i++) {
				if (i == tmp_list.begin())
					cerr << *i;
				else
					cerr << ", " << *i;
			}
			cerr << ")";
			cerr << " got a score of " << score << endl;
		}
		group_scores[score].push_back(tmp_list); // Push back the score along with the corresponding group
		DEBUG {
			cerr << "Made group of ";
			for (auto i : tmp_list) {
				cerr << name_bimap.right.at(i) << ", ";
			}
			cerr << endl;
		}
		scores.insert(score);
	}
	for (auto i = scores.rbegin(); i != scores.rend(); i++) { // need to iterate backwards as sets are sorted lowest to highest
		vector<list<int> > score_tier = group_scores[*i];
		unsigned r;
		while (score_tier.size()) {
			r = rand()%score_tier.size();
			list<int> l = score_tier[r];
			if (!check_add_group(l, final_groups, person_taken))
				break; // Don't wanna be iterating forever, just a few random choices will do
			else
				score_tier.erase(score_tier.begin()+r);
		}
		for (auto it = score_tier.begin(); it != score_tier.end(); it++) {
			if (check_add_group(*it, final_groups, person_taken))
				it = score_tier.erase(score_tier.begin()+r); // score_tierector::erase returns iterator to next element after the erased
		}
	}
	list<int> outliers;
	for (choice c : gs) {
		if (person_taken[c.chooser] == false)
			outliers.push_back(c.chooser);
	}

	// Only thing left is outputting the results
	for (auto group : final_groups) {
		for (auto person : group) {
			cout << name_bimap.right.at(person) << endl;
		}
		cout << endl;
	}
	cout << endl;
	for (auto outlier : outliers) {
		cout << name_bimap.right.at(outlier) << endl;
	}
	delete[] person_taken;
	return 0;
}
