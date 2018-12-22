#include "CreationScreen.h"

CreationScreen::CreationScreen(QWidget* parent, const fs::path path) : QDialog(parent) {
	if (path == "") {
		std::cout << "Map Creation attempted with bad path\n";
		return;
	}
	ui.setupUi(this);
	//setAttribute(Qt::WA_DeleteOnClose);

	QLineEdit *height = new QLineEdit("height");
	QLineEdit *width = new QLineEdit("width");
	height->setValidator(new QIntValidator(0, 512, this));
	width->setValidator(new QIntValidator(0, 512, this));

	connect(ui.buttonBox, &QDialogButtonBox::accepted, [&, path]() {
		save(path);
		emit accept();
		close();
	});

	connect(ui.buttonBox, &QDialogButtonBox::rejected, [&]() {
		emit reject();
		close();
	});
	show();
}

void CreationScreen::save(const fs::path path) const {
	int width = ui.width->text().toInt();
	int height = ui.height->text().toInt();
	int mapsize16 = height * width * 16;
	//int lua = ui.lua->text();
	int lua = 0;

	char tileset = 'X'; //maybe implement tileset choice panel; it will be a lot of work

	BinaryWriter w3i;
	w3i.write(28); //TFT file version number
	w3i.write(0); //Map Version
	w3i.write(6072); //Editor version
	w3i.write(1); //major version of 1.31
	w3i.write(0x1F); //minor version of 1.31
	w3i.write(1); //patch version of 1.31
	w3i.write(0x2F84); //build version of 1.31
	w3i.write_c_string(ui.name->text().toStdString()); //TODO:WTS
	w3i.write_c_string(ui.author->text().toStdString()); //TODO:WTS
	w3i.write_c_string(ui.description->toPlainText().toStdString()); //TODO:WTS
	w3i.write_c_string(ui.suggestedPlayers->text().toStdString()); //TODO:WTS
	float temp = 0.f;
	for (int i = 0; i < 8; i++)
		w3i.write(temp);
	for (int i = 0; i < 4; i++)
		w3i.write(0);
	w3i.write(width);
	w3i.write(height);
	w3i.write(0x0004); //map flags
	w3i.write(tileset);
	w3i.write(-1);
	for (int i = 0; i < 4; i++)
		w3i.write_c_string("");  //load screen strings
	w3i.write(0); //game data set
	for (int i = 0; i < 4; i++)
		w3i.write_c_string("");  //Prologue Strings
	w3i.write(0); //terrain fog
	for (int i = 0; i < 3; i++)
		w3i.write(temp);
	for (int i = 0; i < 4; i++)
		w3i.write((char)0);
	w3i.write(0); //weather id
	w3i.write_c_string(""); // custom sound environment
	w3i.write(tileset);
	for (int i = 0; i < 3; i++)
		w3i.write((char)0);
	w3i.write((char)1);
	w3i.write(lua);
	w3i.write(1); //one player atm
	
	w3i.write(0);
	w3i.write(1); //type:human controlled
	w3i.write(4); //race
	w3i.write(0);
	w3i.write_c_string("Player0"); //TODO:WTS
	w3i.write(temp);
	w3i.write(temp);
	w3i.write(0);
	w3i.write(0);

	for (int i = 0; i < 5; i++)
		w3i.write(0);

	BinaryWriter w3e;
	w3e.write_string("W3E!");
	w3e.write(11);
	w3e.write(tileset);
	w3e.write(1);
	w3e.write(9);
	w3e.write_string("XdrtXdtrXblmXbtlXsqdXrtlXgsbXhdgXwmb");
	w3e.write(2);
	w3e.write_string("CXdiCXsq");
	w3e.write(width + 1);
	w3e.write(height + 1);
	float centx = -1 * width * 128 / 2;
	float centy = -1 * height * 128 / 2;
	w3e.write(centx);
	w3e.write(centy);

	for (int i = 0; i < (height + 1)*(width + 1); i++)
	{
		w3e.write((uint32_t)0);
		w3e.write((byte)0);
		w3e.write((byte)0);
		w3e.write((byte)0);
	}

	BinaryWriter wpm;
	wpm.write_string("MP3W");
	wpm.write(0);
	wpm.write(width * 4);
	wpm.write(height * 4);

	for (int i = 0; i < mapsize16; i++) //4x4 pixels per tile
	{
		wpm.write((char)0x40);
	}

	BinaryWriter doo;
	doo.write_string("W3do");
	doo.write(8);
	doo.write(11);
	doo.write(0);
	doo.write(0);
	doo.write(0);

	BinaryWriter udoo;
	udoo.write_string("W3do");
	udoo.write(8);
	udoo.write(11);
	udoo.write(1);

	udoo.write_string("sloc");
	udoo.write(0);
	udoo.write((float)0);
	udoo.write((float)0);
	udoo.write((float)64);
	udoo.write(270.f);
	for (int i = 0; i < 3;i++)
		//udoo.write(128.f); //scale,WC3?
		udoo.write(1.f); //scale,WE?
	udoo.write((char)2);
	udoo.write(0);
	udoo.write((char)0);
	udoo.write((char)0);
	udoo.write(-1);
	udoo.write(-1);
	udoo.write(-1);
	udoo.write(0);
	udoo.write(12500);
	udoo.write((float)-1);
	udoo.write(1);
	for(int i=0;i<7;i++)
		udoo.write(0);
	udoo.write(-1);
	udoo.write(-1);
	udoo.write(0);
	
	BinaryWriter shd;
	for(int i=0; i< mapsize16;i++) //4x4 pixels per tile
		shd.write((char)0);

	BinaryWriter mmp;
	mmp.write(0);
	mmp.write(1);
	mmp.write(2);
	mmp.write(50);
	mmp.write(50);
	mmp.write((char)0x03);
	mmp.write((char)0x03);
	mmp.write((char)0xFF);
	mmp.write((char)0xFF);

	BinaryWriter mapj;
	mapj.write_string("globals\r\n");
	mapj.write_string("endglobals\r\n\r\n");
	
	mapj.write_string("function InitGlobals takes nothing returns nothing\r\n");
	mapj.write_string("endfunction\r\n\r\n");

	mapj.write_string("function InitCustomPlayerSlots takes nothing returns nothing\r\n");
	mapj.write_string("\tcall SetPlayerStartLocation(Player(0), 0)\r\n");
	mapj.write_string("\tcall SetPlayerColor(Player(0), ConvertPlayerColor(0))\r\n");
	mapj.write_string("\tcall SetPlayerRacePreference(Player(0), RACE_PREF_NIGHTELF)\r\n");
	mapj.write_string("\tcall SetPlayerRaceSelectable(Player(0), true)\r\n");
	mapj.write_string("\tcall SetPlayerController(Player(0), MAP_CONTROL_USER)\r\n");
	mapj.write_string("endfunction\r\n\r\n");

	mapj.write_string("function InitCustomTeams takes nothing returns nothing\r\n");
	mapj.write_string("endfunction\r\n\r\n");

	mapj.write_string("function main takes nothing returns nothing\r\n");
	//mapj.write_string("\tcall SetCameraBounds(- 3328.0 + GetCameraMargin(CAMERA_MARGIN_LEFT), - 3584.0\
	+ GetCameraMargin(CAMERA_MARGIN_BOTTOM), 3328.0 - GetCameraMargin(CAMERA_MARGIN_RIGHT), 3072.0 - GetCameraMargin(CAMERA_MARGIN_TOP),\
	- 3328.0 + GetCameraMargin(CAMERA_MARGIN_LEFT), 3072.0 - GetCameraMargin(CAMERA_MARGIN_TOP), 3328.0 - GetCameraMargin(CAMERA_MARGIN_RIGHT),\
	- 3584.0 + GetCameraMargin(CAMERA_MARGIN_BOTTOM))\r\n");//TODO:map size changes affect this line
	mapj.write_string("\tcall SetDayNightModels(\"Environment\\\\DNC\\\\DNCDalaran\\\\DNCDalaranTerrain\\\\DNCDalaranTerrain.mdl\", \"Environment\\\\DNC\\\\DNCDalaran\\\\DNCDalaranUnit\\\\DNCDalaranUnit.mdl\")\r\n");
	mapj.write_string("\tcall NewSoundEnvironment(\"Default\")\r\n");
	mapj.write_string("\tcall SetAmbientDaySound(\"DalaranDay\")\r\n");
	mapj.write_string("\tcall SetAmbientNightSound(\"DalaranNight\")\r\n");
	mapj.write_string("\tcall SetMapMusic(\"Music\", true, 0)\r\n");
	mapj.write_string("\tcall InitBlizzard()\r\n");
	mapj.write_string("\tcall InitGlobals()\r\n");
	mapj.write_string("endfunction\r\n\r\n");
	
	mapj.write_string("function config takes nothing returns nothing\r\n");
	mapj.write_string("\tcall SetMapName(\"" + ui.name->text().toStdString() + "\")\r\n"); //TODO:WTS?
	mapj.write_string("\tcall SetMapDescription(\"" + ui.description->toPlainText().toStdString() + "\")\r\n"); //TODO:WTS?
	mapj.write_string("\tcall SetPlayers(1)\r\n");
	mapj.write_string("\tcall SetTeams(1)\r\n");
	mapj.write_string("\tcall SetGamePlacement(MAP_PLACEMENT_USE_MAP_SETTINGS)\r\n");
	mapj.write_string("\tcall DefineStartLocation(0, 0.0, 0.0)\r\n");
	mapj.write_string("\tcall InitCustomPlayerSlots()\r\n");
	mapj.write_string("\tcall SetPlayerSlotAvailable(Player(0), MAP_CONTROL_USER)\r\n");
	mapj.write_string("\tcall InitGenericPlayerSlots()\r\n");
	mapj.write_string("endfunction");
	

	BinaryWriter wct;
	wct.write(0x80000004);
	wct.write(1);
	wct.write_c_string("");
	wct.write(0);

	BinaryWriter w3c;
	w3c.write(0);
	w3c.write(0);

	BinaryWriter w3r;
	w3r.write(5);
	w3r.write(0);

	/*BinaryWriter w3s; //gets deleted when empty?
	w3s.write(1);
	w3s.write(0);*/

	BinaryWriter wts;
	wts.write((byte)0xEF);
	wts.write((byte)0xBB);
	wts.write((byte)0xBF);

	BinaryWriter wtg;
	wtg.write_string("WTG!");
	wtg.write(0x80000004);
	wtg.write(7);
	wtg.write(1);
	wtg.write(0);
	wtg.write(0);
	wtg.write(0);
	wtg.write(0);
	wtg.write(0);
	wtg.write(0);
	wtg.write(0);
	wtg.write(0);
	wtg.write(0);
	wtg.write(0);
	wtg.write(0);
	wtg.write(0);
	wtg.write(0);
	wtg.write(0);
	wtg.write(0);
	wtg.write(2);
	wtg.write(0);
	wtg.write(1);
	wtg.write(1);
	wtg.write(0);
	wtg.write_c_string(path.filename().string());
	wtg.write(0);
	wtg.write(0);
	wtg.write(0xFFFFFFFF);

	std::ofstream w3i_file(path / "war3map.w3i", std::ios::binary);
	w3i_file.write(reinterpret_cast<char const*>(w3i.buffer.data()), w3i.buffer.size());
	w3i_file.close();

	std::ofstream w3e_file(path / "war3map.w3e", std::ios::binary);
	w3e_file.write(reinterpret_cast<char const*>(w3e.buffer.data()), w3e.buffer.size());
	w3e_file.close();

	std::ofstream wpm_file(path / "war3map.wpm", std::ios::binary);
	wpm_file.write(reinterpret_cast<char const*>(wpm.buffer.data()), wpm.buffer.size());
	wpm_file.close();

	std::ofstream doo_file(path / "war3map.doo", std::ios::binary);
	doo_file.write(reinterpret_cast<char const*>(doo.buffer.data()), doo.buffer.size());
	doo_file.close();

	std::ofstream udoo_file(path / "war3mapUnits.doo", std::ios::binary);
	udoo_file.write(reinterpret_cast<char const*>(udoo.buffer.data()), udoo.buffer.size());
	udoo_file.close();

	std::ofstream shd_file(path / "war3map.shd", std::ios::binary);
	shd_file.write(reinterpret_cast<char const*>(shd.buffer.data()), shd.buffer.size());
	shd_file.close();

	std::ofstream mmp_file(path / "war3map.mmp", std::ios::binary);
	mmp_file.write(reinterpret_cast<char const*>(mmp.buffer.data()), mmp.buffer.size());
	mmp_file.close();

	std::ofstream mapj_file(path / "war3map.j", std::ios::binary);
	mapj_file.write(reinterpret_cast<char const*>(mapj.buffer.data()), mapj.buffer.size());
	mapj_file.close();

	std::ofstream wct_file(path / "war3map.wct", std::ios::binary);
	wct_file.write(reinterpret_cast<char const*>(wct.buffer.data()), wct.buffer.size());
	wct_file.close();

	std::ofstream w3c_file(path / "war3map.w3c", std::ios::binary);
	w3c_file.write(reinterpret_cast<char const*>(w3c.buffer.data()), w3c.buffer.size());
	w3c_file.close();

	std::ofstream w3r_file(path / "war3map.w3r", std::ios::binary);
	w3r_file.write(reinterpret_cast<char const*>(w3r.buffer.data()), w3r.buffer.size());
	w3r_file.close();

	/*std::ofstream w3s_file(path / "war3map.w3s", std::ios::binary);
	w3s_file.write(reinterpret_cast<char const*>(w3s.buffer.data()), w3s.buffer.size());
	w3s_file.close();*/

	std::ofstream wtg_file(path / "war3map.wtg", std::ios::binary);
	wtg_file.write(reinterpret_cast<char const*>(wtg.buffer.data()), wtg.buffer.size());
	wtg_file.close();

	std::ofstream wts_file(path / "war3map.wts", std::ios::binary);
	wts_file.write(reinterpret_cast<char const*>(wts.buffer.data()), wts.buffer.size());
	wts_file.close();
}
