//https://github.com/tmsbrg/adventure3d/blob/master/src/main.cpp	-source
//https://open-foundry.com/fonts/terminal_grotesque_open			-font
#include <stdio.h>
#include <stdint.h>
#include <iostream>
#include <math.h>
#include <SFML/Graphics.hpp>
#include <unordered_map>

using namespace std;
using namespace sf;
const int screenWidth = 1280;
const int screenHeight = 720;
const float cameraHeight = 0.66f; //hode høyde, 1 er taket 0 er gulvet
const int texture_size = 1024; //bredde og høyde til texture arket (alle vegg texturer)
const int texture_wall_size = 128; // bredde og høyde til individuelle vegg texturer
const float fps_refresh_time = 0.1f; // tida mellom FPS text refresh
// vegg textur liste i rekkefølgen de vises på textur arket
enum class WallTexture {
	Wood,
	Metal,
	//o.s.v
};
//assigner symboler til vegg texturer
const std::unordered_map<char, WallTexture> wallTypes{
		{'#', WallTexture::Wood},
		{'$', WallTexture::Metal},
};
//kart størrelse
const int mapWidth = 12;
const int mapHeight = 12;
//Kartet	//skal egt ligge i en egen fil...
const char worldMap[] =
"$$$$$$$$$$$$"
"$..........$"
"$.......#..$"
"$...#...#..$"
"$...#...#..$"
"$...#####..$"
"$..........$"
"$.########.$"
"$..........$"
"$..........$"
"$..........$"
"$$$$$$$$$$$$";

//skaffer en tile fra kartet
char getTile(int x, int y) {
	return worldMap[y * mapWidth + x];
}

//sjekker for error i kartet. TRUE er good, FALSE ved error.
bool mapCheck() {
	//sjekk størrelse
	int mapSize = sizeof(worldMap) - 1; //-1 fordi sizeof tar med NULL characteren
	if (mapSize != mapWidth * mapHeight) {
		std::cout << "MapSize is incorrect";
		return false;
	}
	for (int y = 0; y < mapHeight; y++) {
		for (int x = 0; x < mapWidth; x++) {
			char tile = getTile(x, y);
			//sjekker at tile er gyldig
			if (tile != '.' && wallTypes.find(tile) == wallTypes.end()) {
				cout << "Map tile [" << x << "," << y << "] uses invalid symbol (" << tile << ")" << endl;
				return false;
			}
			//sørg for at kantene er vegger
			if ((y == 0 || x == 0 || y == mapHeight - 1 || x == mapWidth - 1) && tile == '.') {
				cout << "Map edge [" << x << "," << y << "] is floor ('.'). Should be wall." << endl;
			}
		}
	}
	return true;
}

// kollisjon, box med posisjon i senter
// sjekk om hitbox er inne i vegg eller utenfor kartet
bool canMove(Vector2f position, Vector2f size) {
	//lager hjørnenene på boxen
	Vector2i upper_left(position - size / 2.0f);
	Vector2i lower_right(position + size / 2.0f);
	if (upper_left.x < 0 || upper_left.y < 0 || lower_right.x >= mapWidth || lower_right.y >= mapHeight) {
		return false; // out of bounds
	}
	//gå gjennom hver tile inne i boxen, kan inneholde mer enn en.
	for (int y = upper_left.y; y <= lower_right.y; y++) {
		for (int x = upper_left.x; x <= lower_right.x; x++) {
			if (getTile(x, y) != '.') {
				return false;
			}
		}
	}
	return true;
}

//roter vector med gitt verdi i radianer og returner resultatet
//https://en.wikipedia.org/wiki/Rotation_matrix#In_two_dimensions
Vector2f rotateVec(Vector2f vec, float value) {
	return Vector2f(
		vec.x * cos(value) - vec.y * sin(value),
		vec.x * sin(value) + vec.y * cos(value)
	);
}

int main() {
	if (!mapCheck()) {
		cout << "Invalid Map. Exiting..." << endl;
		return EXIT_FAILURE;
	}

	Font font;
	if (!font.loadFromFile("font/terminal-grotesque_open.otf")) {
		cout << "Font not found" << endl;
		return EXIT_FAILURE;
	}

	Texture texture;
	if (!texture.loadFromFile("textures/walls.png")) {
		cout << "Texture not found" << endl;
		return EXIT_FAILURE;
	}

	//render state som bruker texturen
	RenderStates state(&texture);

	//Spiller
	Vector2f position(2.5f, 2.0f); //pos på kartet
	Vector2f direction(0.0f, 1.0f); //dir iforhold til (0.0)
	Vector2f plane(-0.66f, 0.0f);    //raycaster version av cameraplanet
											//MÅ være vinkelrett ift rotasjon

	float size_f = 0.375f; //størrelse på hitbox (i tiles per sec)
	float moveSpeed = 5.0f; //speed (i tiles per sec)
	float rotateSpeed = 3.0f; //---II---

	Vector2f size(size_f, size_f); //Hitbox bredde og høyde

	//lag vindu
	RenderWindow window(VideoMode(screenWidth, screenHeight), "Calamity"); //+1 på screenWidth fixxer en bug, idk y
	window.setSize(Vector2u(screenWidth, screenHeight));

	window.setFramerateLimit(60);
	bool hasFocus = true;

	//linjene for vegger og gulv
	VertexArray lines(Lines, 18 * screenWidth);

	Text fpsText("", font, 50); //FPS display
	Clock clock;
	char frameInfoString[sizeof("FPS: *****.*, Frame time: ******")]; //string buffer for frame info	//ikkje heilt sikker på ka an gjer

	float dt_counter = 0.0f; //delta tid for flere frames for å regne ut FPS riktig
	int frame_counter = 0;
	int64_t frame_time_micro = 0; //tiden nødevendig for å tegne frames i microsec

	while (window.isOpen()) {

		//mekk delta time
		float dt = clock.restart().asSeconds();

		//oppdater FPS, smuud over tid
		if (dt_counter >= fps_refresh_time) {
			float fps = (float)frame_counter / dt_counter;
			frame_time_micro /= frame_counter;
			snprintf(frameInfoString, sizeof(frameInfoString), "FPS: %3.1f, Frame Time: %61d", fps, frame_time_micro);
			fpsText.setString(frameInfoString);
			dt_counter = 0.0f;
			frame_counter = 0;
			frame_time_micro = 0;
		}

		dt_counter += dt;
		++frame_counter;

		//SFML eventer
		Event event;
		while (window.pollEvent(event)) {
			switch (event.type) {
			case Event::Closed:
				window.close();
				break;
			case Event::KeyPressed:
				if (event.key.code == Keyboard::Escape) {
					window.close();
				} break;
			case Event::LostFocus:
				hasFocus = false;
				break;
			case Event::GainedFocus:
				hasFocus = true;
				break;
			}
		}

		//controls
		if (hasFocus) {
			using key = Keyboard;

			float moveForward = 0.0f;
			float moveStrafe = 0.0f;

			//input
			if (key::isKeyPressed(key::W)) {
				moveForward = 1.0f;
			}
			else if (key::isKeyPressed(key::S)) {
				moveForward = -1.0f;
			}
			//strafe
			else if (key::isKeyPressed(key::A)) {
				moveStrafe = -1.0f;
			}
			else if (key::isKeyPressed(key::D)) {
				moveStrafe = 1.0f;
			}
			if (key::isKeyPressed(key::W) && key::isKeyPressed(key::A)) {
				moveStrafe = -0.75f;
				moveForward = 0.75f;
			}
			else if (key::isKeyPressed(key::W) && key::isKeyPressed(key::D)) {
				moveStrafe = 0.75f;
				moveForward = 0.75f;
			}
			if (key::isKeyPressed(key::S) && key::isKeyPressed(key::A)) {
				moveStrafe = -0.75f;
				moveForward = -0.75f;
			}
			else if (key::isKeyPressed(key::S) && key::isKeyPressed(key::D)) {
				moveStrafe = 0.75f;
				moveForward = -0.75f;
			}

			//håndter bevegelse
			//frem tilbake
			if (moveForward != 0.0f) {
				Vector2f moveVec = direction * moveSpeed * moveForward * dt;

				if (canMove(Vector2f(position.x + moveVec.x, position.y), size)) {
					position.x += moveVec.x;
				}
				if (canMove(Vector2f(position.x, position.y + moveVec.y), size)) {
					position.y += moveVec.y;
				}
			}

			//rotasjon
			float rotateDirection = 0.0f;

			//mekk input
			if (key::isKeyPressed(key::Left)) {
				rotateDirection = -1.0f;
			}
			else if (key::isKeyPressed(key::Right)) {
				rotateDirection = 1.0f;
			}
			//håndtere rotasjon
			if (rotateDirection != 0.0f) {
				float rotation = rotateSpeed * rotateDirection * dt;
				direction = rotateVec(direction, rotation);
				plane = rotateVec(plane, rotation);
			}
			//STRAAAFE

			if (moveStrafe != 0.0f) {
				Vector2f strafeVec = plane * moveSpeed * moveStrafe * dt;

				if (canMove(Vector2f(position.x + strafeVec.x, position.y), size)) {
					position.x += strafeVec.x;
				}
				if (canMove(Vector2f(position.x, position.y + strafeVec.y), size)) {
					position.y += strafeVec.y;
				}
			}
		}

		lines.resize(0);

		//går gjennom vertikale skjerm linjer, tegner linjer for hver
		for (int x = 0; x < screenWidth; ++x) {

			//ray som skal castes ----
			float cameraX = 2 * x / (float)screenWidth - 1.0f; // X i kameraspace (mellom 1 og -1)
			Vector2f rayPos = position;
			Vector2f rayDir = direction + plane * cameraX;

			//regn ut distansen mellom hver grid linje for x og y ifh retning
			Vector2f deltaDist(
				sqrt(1.0f + (rayDir.y * rayDir.y) / (rayDir.x * rayDir.x)),
				sqrt(1.0f + (rayDir.x * rayDir.x) / (rayDir.y * rayDir.y))
				//prøv abs(1 / rayDir.x),
				//     abs(1 / rayDir.y)
			);
			Vector2i mapPos(rayPos); //hvilken boks av kartet vi befinner oss i

			Vector2i step; //hvilken retning vi stepper i (fra 1 -> -1)
			Vector2f sideDist; //Distanse fra nåværende sted til neste gridline, for x og y separat

			//calc step og start sideDist
			if (rayDir.x < 0.0f) {
				step.x = -1;
				sideDist.x = (rayPos.x - mapPos.x) * deltaDist.x;
			}
			else {
				step.x = 1;
				sideDist.x = (mapPos.x + 1.0f - rayPos.x) * deltaDist.x;
			}
			if (rayDir.y < 0.0f) {
				step.y = -1;
				sideDist.y = (rayPos.y - mapPos.y) * deltaDist.y;
			}
			else {
				step.y = 1;
				sideDist.y = (mapPos.y + 1.0f - rayPos.y) * deltaDist.y;
			}

			char tile = '.'; //tile som blir truffet
			bool horizontal; //om siden er horisontal eller vertical

			float perpWallDist = 0.0f; //distanse fra vegg, projektert på kameras retning
			int wallHeight;            //hvor høyt veggen skal tegnes på skjermen ved forskjellige distanser
			int ceilingPixel = 0;      //posisjonen t tak pixl på skjermen
			int groundPixel = screenHeight; // pos bakke pixl

			//farge for gulvet
			//DETTE SKAL GJØRES OM TIL TEXTURE!
			Color color1 = Color::Blue;
			Color color2 = Color::Green;

			Color color = ((mapPos.x % 2 == 0 && mapPos.y % 2 == 0) ||
				(mapPos.x % 2 == 1 && mapPos.y % 2 == 1)) ? color1 : color2;

			//cast Ray til den treffer en vegg, tegn gulv imens
			while (tile == '.') {
				if (sideDist.x < sideDist.y) {
					sideDist.x += deltaDist.x;
					mapPos.x += step.x;
					horizontal = true;
					perpWallDist = (mapPos.x - rayPos.x + (1 - step.x) / 2) / rayDir.x;
				}
				else {
					sideDist.y += deltaDist.y;
					mapPos.y += step.y;
					horizontal = false;
					perpWallDist = (mapPos.y - rayPos.y + (1 - step.y) / 2) / rayDir.y;
				}

				wallHeight = screenHeight / perpWallDist;

				//gulv
				lines.append(Vertex(Vector2f((float)x, (float)groundPixel), color, Vector2f(385.0f, 129.0f)));
				groundPixel = int(wallHeight * cameraHeight + screenHeight * 0.5f);
				lines.append(Vertex(Vector2f((float)x, (float)groundPixel), color, Vector2f(385.0f, 129.0f)));

				//tak
				Color color_c = color;
				color_c.r /= 2;
				color_c.g /= 2;
				color_c.b /= 2;

				lines.append(Vertex(Vector2f((float)x, (float)ceilingPixel), color_c, Vector2f(385.0f, 129.0f)));
				ceilingPixel = int(-wallHeight * (1.0f - cameraHeight) + screenHeight * 0.5f);
				lines.append(Vertex(Vector2f((float)x, (float)ceilingPixel), color_c, Vector2f(385.0f, 129.0f)));

				//skift mellom de to fargene
				color = (color == color1) ? color2 : color1;

				//finn hvilken tile
				tile = getTile(mapPos.x, mapPos.y);
			}

			//calc lavest og høyest pixl for å fylle linja
			int drawStart = ceilingPixel;
			int drawEnd = groundPixel;

			//få pos av vegg textur i full textur
			int wallTextureNum = (int)wallTypes.find(tile)->second;
			Vector2i texture_coords(
				wallTextureNum * texture_wall_size % texture_size,
				wallTextureNum * texture_wall_size / texture_size * texture_wall_size
			);
			//calc hvor veggen blir truffet
			float wall_x;
			if (horizontal) {
				wall_x = rayPos.y + perpWallDist * rayDir.y;
			}
			else {
				wall_x = rayPos.x + perpWallDist * rayDir.x;
			}
			wall_x -= floor(wall_x);

			//få x coords på texturen
			int tex_x = int(wall_x * float(texture_wall_size));

			//flip texture hvis den er på andre side av oss for å forhindre speil effekt
			if ((horizontal && rayDir.x <= 0) || (!horizontal && rayDir.y >= 0)) {
				tex_x = texture_wall_size - tex_x - 1;
			}
			texture_coords.x += tex_x;

			//skygge
			color = Color::White;
			if (horizontal) {
				color.r /= 2;
				color.g /= 2;
				color.b /= 2;
			}

			//legg linje til vertex buffer
			lines.append(Vertex(
				Vector2f((float)x, (float)drawStart),
				color,
				Vector2f((float)texture_coords.x, (float)texture_coords.y + 1)
			));
			lines.append(Vertex(
				Vector2f((float)x, (float)drawEnd),
				color,
				Vector2f((float)texture_coords.x, (float)texture_coords.y + texture_wall_size - 1)
			));

		}

		window.clear();
		window.draw(lines, state);
		window.draw(fpsText);
		frame_time_micro += clock.getElapsedTime().asMicroseconds();
		window.display();
	}
	return EXIT_SUCCESS;
}