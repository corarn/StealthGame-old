#include	"scenemanager.h"
#include	"SceneClassFactory.h"

// “o˜^‚³‚ê‚Ä‚¢‚éƒV[ƒ“‚ð‘S‚Ä”jŠü‚·‚é
void SceneManager::Dispose() 
{
	// “o˜^‚³‚ê‚Ä‚¢‚é‚·‚×‚ÄƒV[ƒ“‚ÌI—¹ˆ—
	for (auto& s : m_scenes) 
	{
		s.second->Dispose();
	}

	m_scenes.clear();
	m_currentSceneName.clear();
}

void SceneManager::SetCurrentScene(std::string currentscenename) 
{
	m_currentSceneName = currentscenename;
	auto obj = SceneClassFactory::getInstance().create(currentscenename);
	obj->Init();
	m_scenes[m_currentSceneName] = std::move(obj);
}

void SceneManager::Init()
{
}

void SceneManager::Draw(uint64_t deltatime)
{

	// Œ»Ý‚ÌƒV[ƒ“‚ð•`‰æ
	m_scenes[m_currentSceneName]->Draw(deltatime);
}

void SceneManager::Update(uint64_t deltatime)
{
	// Œ»Ý‚ÌƒV[ƒ“‚ðXV
	m_scenes[m_currentSceneName]->Update(deltatime);
}