#include "PlayerController.h"
#include "../../system/IScene.h"
#include "../../system/Inputmanager.h"

PlayerController::PlayerController(SceneBase& scene)
	: m_scene(&scene)
{
}

Vector3 PlayerController::DecideMoveDirection()
{
	Vector2 moveInput = ReadMoveInput();
	return ConvertToCameraDirection(moveInput);
}

Vector2 PlayerController::ReadMoveInput() const
{
	const CInputManager& input = CInputManager::GetInstance();
	Vector2 moveInput = Vector2(0.0f, 0.0f);
	if (input.IsKeyPressed(DIK_W))
	{
		moveInput.y += 1.0f;
	}
	if (input.IsKeyPressed(DIK_S))
	{
		moveInput.y -= 1.0f;
	}
	if (input.IsKeyPressed(DIK_D))
	{
		moveInput.x += 1.0f;
	}
	if (input.IsKeyPressed(DIK_A))
	{
		moveInput.x -= 1.0f;
	}
	return moveInput;
}

Vector3 PlayerController::ConvertToCameraDirection(const Vector2& moveInput) const
{
	const Camera* camera = m_scene->GetCamera();

	// カメラはシーンが作る。作る前に呼ばれたら動かさない
	if (camera == nullptr)
	{
		return Vector3(0.0f, 0.0f, 0.0f);
	}

	// 上下の成分を消して、地面に沿わせる（下を向いたカメラでも、地面にもぐらないように）
	Vector3 cameraForward = camera->GetLookat() - camera->GetPosition();
	cameraForward.y = 0.0f;
	cameraForward.Normalize();

	// 上と前の外積が右になる（DirectX は左手座標）
	Vector3 cameraRight = Vector3::Up.Cross(cameraForward);

	Vector3 direction = cameraForward * moveInput.y + cameraRight * moveInput.x;

	// 斜め（W と D）は長さが 1.41 になって速くなるので、1 に戻す。何も押していないときの 0 はそのまま
	if (direction.LengthSquared() > 0.0f)
	{
		direction.Normalize();
	}
	return direction;
}
