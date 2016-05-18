#include "../fastecs.hh"

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
using namespace std;

struct Position {
	COMP_ID(0x0);
	uint8_t x, y;
	Position(uint8_t x, uint8_t y) : x(x), y(y) {}
};

struct Direction {
	COMP_ID(0x1);
	uint8_t angle;
	Direction(uint8_t d) : angle(d) {}
};

struct test_debug {
	template<typename E>
	static void examine(string const& s, E const& e) {
		cout << s << " ->\n";
		cout << "  _entity_index:    [ ";
		for(auto const& i: e._entity_index) { cout << i << " "; }
		cout << "]\n";

		cout << "  _component_index: [ ";
		for(auto const& i: e._components) { 
			cout << hex << uppercase << setfill('0') << setw(2);
			cout << static_cast<int>(i) << " "; 
		}
		cout << "]\n\n";
	}
};

int main()
{
    Engine<> e;
	test_debug::examine("empty engine", e);

	Entity e1 = e.CreateEntity();
	test_debug::examine("entity created", e);

	e.AddComponent<Direction, uint8_t>(e1, 0x50);
	test_debug::examine("component added (Direction=0x50)", e);

	e.AddComponent<Position, uint8_t, uint8_t>(e1, 7, 8);
	test_debug::examine("component added (Position x=7, y=8)", e);

	assert(e.GetComponent<Position>(e1).y == 8);
	assert(e.GetComponent<Direction>(e1).angle == 0x50);
	assert(e.HasComponent<Direction>(e1));

	e.RemoveComponent<Direction>(e1);
	test_debug::examine("component removed (direction)", e);
	assert(!e.HasComponent<Direction>(e1));
	assert(e.GetComponent<Position>(e1).y == 8);
	
	// TODO - add component to a previous entity
}

// vim: ts=4:sw=4:sts=4:expandtab:foldmethod=marker
