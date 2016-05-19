#include "fastecs.hh"

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
using namespace std;

struct Position {
	COMP_ID(0x0);
	uint8_t x, y;
	Position(uint8_t x, uint8_t y) : x(x), y(y) {}
    string to_str() const { return "Position: " + to_string(static_cast<int>(x)) + ", " + to_string(static_cast<int>(y)); }
};

struct Direction {
	COMP_ID(0x1);
	uint8_t angle;
	Direction(uint8_t d) : angle(d) {}
    string to_str() const { return "Direction: " + to_string(static_cast<int>(angle)); }
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
	static void examine(ECS::Engine<> const& e) {
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
		cout << KNRM "]\n";
        cout << e.Examine<Direction, Position>() << "\n";
	}
};

#define M(...) __VA_ARGS__
#define DO(s) s; cout << KCYN << #s ";" << KNRM << "\n"; test_debug::examine(e);

#define ASSERT(s) cout << KCYN << #s ";" << KNRM << "\n"; assert(s);

#define EXPECT(v) { if(e.ComponentVector() != vector<uint8_t>(v)) { cout << KRED "Assertion error!\n"; abort(); } }

int main()
{
	DO(ECS::Engine<> e);
    EXPECT({});
    DO(ECS::Entity e1 = e.CreateEntity());
    EXPECT(M({0x04, 0x00, 0x00, 0x00}));
	DO(e.AddComponent<M(Direction, uint8_t)>(e1, 0x50));
    EXPECT(M({0x09, 0x00, 0x00, 0x00, 0x05, 0x00, 0x01, 0x00, 0x50}));
	DO(e.AddComponent<M(Position, uint8_t, uint8_t)>(e1, 7, 8));
    EXPECT(M({0x0F, 0x00, 0x00, 0x00, 0x05, 0x00, 0x01, 0x00, 0x50, 0x06, 0x00, 0x00, 0x00, 0x07, 0x08}));

	ASSERT(e.GetComponent<Position>(e1).y == 8);
	ASSERT(e.GetComponent<Direction>(e1).angle == 0x50);
	ASSERT(e.HasComponent<Direction>(e1));

	DO(e.GetComponent<Position>(e1).y = 10);
    EXPECT(M({0x0F, 0x00, 0x00, 0x00, 0x05, 0x00, 0x01, 0x00, 0x50, 0x06, 0x00, 0x00, 0x00, 0x07, 0x0A}));
	ASSERT(e.GetComponent<Position>(e1).y == 10);

	DO(e.RemoveComponent<Direction>(e1));
    EXPECT(M({0x0F, 0x00, 0x00, 0x00, 0x05, 0x00, 0xFF, 0xFF, 0x00, 0x06, 0x00, 0x00, 0x00, 0x07, 0x0A}));
	ASSERT(!e.HasComponent<Direction>(e1));
	ASSERT(e.GetComponent<Position>(e1).y == 10);

	DO(ECS::Entity e2 = e.CreateEntity());
    EXPECT(M({0x0F, 0x00, 0x00, 0x00, 0x05, 0x00, 0xFF, 0xFF, 0x00, 0x06, 0x00, 0x00, 0x00, 0x07, 0x0A, 0x04, 0x00, 0x00, 0x00}));
	DO(e.AddComponent<M(Direction, uint8_t)>(e2, 0xAF));
    EXPECT(M({0x0F, 0x00, 0x00, 0x00, 0x05, 0x00, 0xFF, 0xFF, 0x00, 0x06, 0x00, 0x00, 0x00, 0x07, 0x0A, 0x09, 0x00, 0x00, 0x00, 0x05, 0x00, 0x01, 0x00, 0xAF}));

	DO(e.RemoveAllComponents(e1));
    EXPECT(M({0x0F, 0x00, 0x00, 0x00, 0x05, 0x00, 0xFF, 0xFF, 0x00, 0x06, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x05, 0x00, 0x01, 0x00, 0xAF}));

	cout << "------------------------------\n\n";

	DO(e.Reset());

    // add component to middle of list
	DO(e1 = e.CreateEntity(); e2 = e.CreateEntity());
    EXPECT(M({0x04, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00}));
	DO(e.AddComponent<M(Direction, uint8_t)>(e1, 221));
    EXPECT(M({0x09, 0x00, 0x00, 0x00, 0x05, 0x00, 0x01, 0x00, 0xDD, 0x04, 0x00, 0x00, 0x00}));
	DO(e.AddComponent<M(Position, uint8_t, uint8_t)>(e1, 4, 5));
    EXPECT(M({0x0F, 0x00, 0x00, 0x00, 0x05, 0x00, 0x01, 0x00, 0xDD, 0x06, 0x00, 0x00, 0x00, 0x04, 0x05, 0x04, 0x00, 0x00, 0x00}));
	DO(e.AddComponent<M(Direction, uint8_t)>(e2, 123));
    EXPECT(M({0x0F, 0x00, 0x00, 0x00, 0x05, 0x00, 0x01, 0x00, 0xDD, 0x06, 0x00, 0x00, 0x00, 0x04, 0x05, 0x09, 0x00, 0x00, 0x00, 0x05, 0x00, 0x01, 0x00, 0x7B}));

	cout << "------------------------------\n\n";

    // reuse component
    DO(e.Reset(); e1 = e.CreateEntity());
    EXPECT(M({0x04, 0x00, 0x00, 0x00}));
	DO(e.AddComponent<M(Direction, uint8_t)>(e1, 50); e.AddComponent<M(Position, uint8_t, uint8_t)>(e1, 7, 8));
    EXPECT(M({0x0F, 0x00, 0x00, 0x00, 0x05, 0x00, 0x01, 0x00, 0x32, 0x06, 0x00, 0x00, 0x00, 0x07, 0x08}));
	DO(e.RemoveComponent<Direction>(e1));
    EXPECT(M({0x0F, 0x00, 0x00, 0x00, 0x05, 0x00, 0xFF, 0xFF, 0x00, 0x06, 0x00, 0x00, 0x00, 0x07, 0x08}));
	DO(e.AddComponent<M(Direction, uint8_t)>(e1, 24));
    EXPECT(M({0x0F, 0x00, 0x00, 0x00, 0x05, 0x00, 0x01, 0x00, 0x18, 0x06, 0x00, 0x00, 0x00, 0x07, 0x08}));

    // reuse entity
    DO(e2 = e.CreateEntity(); e.AddComponent<M(Direction, uint8_t)>(e2, 22));
    EXPECT(M({0x0F, 0x00, 0x00, 0x00, 0x05, 0x00, 0x01, 0x00, 0x18, 0x06, 0x00, 0x00, 0x00, 0x07, 0x08, 0x09, 0x00, 0x00, 0x00, 0x05, 0x00, 0x01, 0x00, 0x16}));
    DO(e.RemoveEntity(e1));
    EXPECT(M({0x09, 0x00, 0x00, 0x00, 0x05, 0x00, 0x01, 0x00, 0x16}));

	cout << "------------------------------\n\n";
	
	// TODO - iteration
	
	// TODO - systems
}

// vim: ts=4:sw=4:sts=4:expandtab:foldmethod=marker
