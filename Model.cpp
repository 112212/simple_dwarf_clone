
#include "Model.hpp"
#include "libs/json.hpp"
#include <fstream>

Model::Model() {
	m_view = ViewType::menu;
}

Model::Chunk& Model::GetChunk(glm::ivec2 pos) {
	return chunks[pos];
}


void Model::GenerateMap() {
	
	// generate empty map
	for(int x=50; x < 100; x++)
	for(int y=50; y < 100; y++)
		chunks[glm::ivec2(x,y)] = Chunk();
	
	// put player on map
	player = std::make_shared<Actor>();
	glm::ivec2 playerPos = {75*8,75*8};
	auto &plpos = GetObjectAt(playerPos);
	plpos.type = Object::Type::friendly;
	plpos.obj = player;
	player->position = playerPos;
}

void Model::MovePos(glm::ivec2 oldPos, glm::ivec2 newPos) {
	auto &place = GetObjectAt(oldPos);
	auto &placeToGo = GetObjectAt(newPos);
	place.obj.swap(placeToGo.obj);
	place.obj.reset();
	static_cast<Actor*>(placeToGo.obj.get())->position = newPos;
}

std::shared_ptr<Actor> Model::GetPlayer() {
	return player;
}

glm::ivec2 Model::GetPlayerPosition() {
	return player->position;
}


Tile& Model::GetObjectAt(glm::ivec2 pos) {
	return GetChunk( pos / 8 ).ObjectAt(pos % 8);
}


static nlohmann::json ActorToJson(Actor* actor) {
	using namespace nlohmann;
	json jactor;
	jactor["position"] = json::array({actor->position.x, actor->position.y });
	jactor["hp"] = actor->hp;
	jactor["armor"] = actor->armor;
	jactor["damage"] = actor->armor;
	
	jactor["items"] = json::array();
	
	for(auto& itm : actor->items) {
		json jitem;
		jitem["charRepr"] = itm->charRepr;
		// jitem["
		jactor["items"].push_back(jitem);
	}
	return jactor;
}

void Model::SaveGame(std::string jsonFilename) {
	using namespace nlohmann;
	json j;
	
	json jplayer;
	jplayer["position"] = json::array({player->position.x, player->position.y });
	jplayer["hp"] = player->hp;
	jplayer["armor"] = player->armor;
	jplayer["damage"] = player->armor;
	
	
	j["player"] = ActorToJson(player.get());
	
	
	
	std::fstream f(jsonFilename);
	f << j;
}

void Model::LoadGame(std::string jsonFilename) {
	using namespace nlohmann;
	std::fstream f(jsonFilename);
	json j;
	f >> j;
	
	json jpos = j["player"]["position"];
	player->position = glm::ivec2(jpos[0], jpos[1]);
	
	
}

void Model::LoadItems(std::string jsonFilename) {
	
}


void Model::SetView(ViewType view) {
	m_view = view;
}

ViewType  Model::GetView() {
	return m_view;
}
