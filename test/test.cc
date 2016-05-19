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

#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"

struct test_debug {
	template<typename E>
	static void examine(string const& s, E const& e) {
		cout << s << " ->\n";
		cout << "  _entity_index:    [ ";
		for(auto const& i: e._entity_index) { cout << i << " "; }
		cout << "]\n";

		cout << "  _component_index: [ ";
        size_t i = 0;
        while(i < e._components.size()) {

            // entity size
            cout << KNRM;
            uint32_t sz; memcpy(&sz, &e._components[i], sizeof(uint32_t));
            for(size_t j=0; j<sizeof(uint32_t); ++i, ++j) {
                cout << hex << uppercase << setfill('0') << setw(2) << static_cast<int>(e._components[i]) << " "; 
            }

            if(i == e._components.size()) {
                break;
            }
            
            // component size
            size_t j = i;
            while(j < (i+sz-sizeof(uint32_t))) {
                uint16_t csz; memcpy(&csz, &e._components[j], sizeof(uint16_t));
                csz = csz - 2*sizeof(uint16_t);

                cout << KBLU;
                for(size_t k=0; k<sizeof(uint16_t); ++j, ++k) {
                    cout << hex << uppercase << setfill('0') << setw(2) << static_cast<int>(e._components[j]) << " "; 
                }

                cout << KMAG;
                for(size_t k=0; k<sizeof(uint16_t); ++j, ++k) {
                    cout << hex << uppercase << setfill('0') << setw(2) << static_cast<int>(e._components[j]) << " "; 
                }

                cout << KGRN;
                for(size_t k=0; k<csz; ++j, ++k) {
                    cout << hex << uppercase << setfill('0') << setw(2) << static_cast<int>(e._components[j]) << " "; 
                }
            }
            i = j;
        }
		cout << KNRM "]\n\n";
	}
};

int main()
{
	ECS::Engine<> e;
	test_debug::examine("empty engine", e);

	ECS::Entity e1 = e.CreateEntity();
	test_debug::examine("entity 1 created", e);

	e.AddComponent<Direction, uint8_t>(e1, 0x50);
	test_debug::examine("component added (Direction=0x50)", e);

	e.AddComponent<Position, uint8_t, uint8_t>(e1, 7, 8);
	test_debug::examine("component added (Position x=7, y=8)", e);

	assert(e.GetComponent<Position>(e1).y == 8);
	assert(e.GetComponent<Direction>(e1).angle == 0x50);
	assert(e.HasComponent<Direction>(e1));

	e.GetComponent<Position>(e1).y = 10;
	assert(e.GetComponent<Position>(e1).y == 10);

	e.RemoveComponent<Direction>(e1);
	test_debug::examine("component removed (direction)", e);
	assert(!e.HasComponent<Direction>(e1));
	assert(e.GetComponent<Position>(e1).y == 10);

	ECS::Entity e2 = e.CreateEntity();
	test_debug::examine("entity 2 created", e);

	e.AddComponent<Direction, uint8_t>(e2, 0xAF);
	test_debug::examine("component added to entity 2 (Direction=0xAF)", e);

	e.RemoveAllComponents(e1);
	test_debug::examine("all components removed from e1", e);

	cout << "------------------------------\n\n";

	e.Reset();
	test_debug::examine("empty engine", e);

	e1 = e.CreateEntity();
	e2 = e.CreateEntity();
	test_debug::examine("two entities created", e);

	e.AddComponent<Direction, uint8_t>(e1, 0xFB);
	test_debug::examine("direction added to e1", e);

	e.AddComponent<Position, uint8_t, uint8_t>(e1, 4, 5);
	test_debug::examine("position added to e1", e);

	e.AddComponent<Direction, uint8_t>(e2, 0xAB);
	test_debug::examine("direction added to e2", e);

	/*
	e1 = e.CreateEntity();
	test_debug::examine("entity 1 created", e);

	e.AddComponent<Direction, uint8_t>(e1, 0x50);
	e.AddComponent<Position, uint8_t, uint8_t>(e1, 7, 8);
	test_debug::examine("component added (Direction=0x50, Position x=7, y=8)", e);
	
	e.RemoveComponent<Direction>(e1);
	test_debug::examine("direction removed", e);
	
	e.AddComponent<Direction, uint8_t>(e1, 0x24);
	test_debug::examine("direction readded", e);  // TODO - reuse component

	cout << "------------------------------\n\n";
	*/

	// TODO - reuse entity
	
	// TODO - iteration
	
	// TODO - systems
}

// vim: ts=4:sw=4:sts=4:expandtab:foldmethod=marker
