#include <unordered_map>
// Bastion: SA-MP objects (CreateObject / mapping). Upstream GTA-2.11 left the
// constructor, destructor and Process empty and the RPC handlers commented out,
// so no server object was ever shown on the phone.
// Objects that have SetObjectMaterial textures/colours, by their game entity
// (the render hook in hooks.cpp looks them up here instead of scanning the pool).
std::unordered_map<void*, CObject*> g_BastionMaterialObjects;

CObject::CObject(int iModel, CVector vecPos, CVector vecRot, float fDrawDistance, uint8_t bAttached)
{
	m_AttachedVehicleID = INVALID_VEHICLE_ID;
	m_AttachedObjectID = INVALID_OBJECT_ID;
	m_vecAttachedPos = CVector{ 0.0f, 0.0f, 0.0f };
	m_vecAttachedRot = CVector{ 0.0f, 0.0f, 0.0f };
	m_bSyncRotation = true;
	m_MaterialTextIndex = 0;
	for (int i = 0; i < 16; i++)
	{
		m_MaterialTexture[i] = 0;
		m_dwMaterialColor[i] = 0;
		m_MaterialTextTexture[i] = 0;
		m_iMaterialType[i] = 0;
		m_szMaterialText[i] = nullptr;
		m_iMaterialSize[i] = 0;
		m_iMaterialFontSize[i] = 0;
		m_dwMaterialFontColor[i] = 0;
		m_dwMaterialBackColor[i] = 0;
		m_iMaterialTextAlign[i] = 0;
	}
	m_bHasMaterial = false;
	m_bHasMaterialText = false;
	m_bAttachedToPed = bAttached;
	m_bForceRender = false;
	m_pEntity = nullptr;
	m_byteMoving = 0;
	m_dwGTAId = 0;
	m_vecRotation = vecRot;
	m_iModel = iModel;
	m_fMoveSpeed = 0.0f;
	m_bNeedRotate = false;
	m_fDistanceToTargetPoint = 0.0f;
	m_dwMoveTick = 0;

	// A model this client does not have (SA-MP 0.3.7 objects 18631+ missing from
	// the mobile cache) is skipped instead of crashing the game.
	if (iModel < 0 || iModel >= CModelInfo::NUM_MODEL_INFOS || CModelInfo::GetModelInfo(iModel) == nullptr)
	{
		Log("[Bastion] object model %d is not in this client, skipped", iModel);
		return;
	}
	if (!pGame->IsModelLoaded(iModel))
	{
		pGame->RequestModel(iModel);
		pGame->LoadRequestedModels();
		if (!pGame->IsModelLoaded(iModel))
		{
			Log("[Bastion] object model %d failed to load, skipped", iModel);
			return;
		}
	}
	// create_object reads the model's collision bounds (CEntity::GetDistanceFrom-
	// CentreOfMassToBaseOfModel); a model without collision in this client crashed
	// the game right after connecting (SIGSEGV at 0x8).
	if (CModelInfo::GetModelInfo(iModel)->GetColModel() == nullptr)
	{
		Log("[Bastion] object model %d has no collision in this client, skipped", iModel);
		return;
	}

	// ScriptCommand's output slots are uintptr-wide on ARM64, but m_dwGTAId is 32-bit.
	// Use a correctly sized local slot to avoid overwriting the next object field.
	uintptr objectHandle = 0;
	ScriptCommand(&create_object, iModel, vecPos.x, vecPos.y, vecPos.z, &objectHandle);
	m_dwGTAId = static_cast<uint32_t>(objectHandle);
	if (!m_dwGTAId) return;
	ScriptCommand(&put_object_at, m_dwGTAId, vecPos.x, vecPos.y, vecPos.z);
	m_pEntity = GamePool_Object_GetAt(m_dwGTAId);
	if (!m_pEntity) return;

	// SA-MP objects are visible in every interior (area 13 = all areas).
	m_pEntity->m_nAreaCode = AREA_CODE_13;
	InstantRotate(vecRot.x, vecRot.y, vecRot.z);
}

CObject::~CObject()
{
	if (m_pEntity) g_BastionMaterialObjects.erase(m_pEntity);
	if (m_pEntity && GamePool_Object_GetAt(m_dwGTAId))
		ScriptCommand(&destroy_object, m_dwGTAId);
	m_pEntity = nullptr;
	for (int i = 0; i < 16; i++)
	{
		if (m_szMaterialText[i] != nullptr) {
			delete[] m_szMaterialText[i];
			m_szMaterialText[i] = nullptr;
		}
	}
}

// MoveObject: straight line at m_fMoveSpeed units/s, rotation interpolated.
void CObject::Process(float fElapsedTime)
{
	if (!m_pEntity || !(m_byteMoving & 1) || !GamePool_Object_GetAt(m_dwGTAId)) return;

	CVector pos = m_pEntity->GetPosition();
	float dx = m_matTarget.pos.x - pos.x;
	float dy = m_matTarget.pos.y - pos.y;
	float dz = m_matTarget.pos.z - pos.z;
	float remaining = sqrtf(dx * dx + dy * dy + dz * dz);
	float step = m_fMoveSpeed * (fElapsedTime > 0.0f ? fElapsedTime : 0.0f);
	bool done = (step >= remaining || remaining < 0.001f || m_fMoveSpeed <= 0.0f);
	if (done)
	{
		pos.x = m_matTarget.pos.x;
		pos.y = m_matTarget.pos.y;
		pos.z = m_matTarget.pos.z;
		remaining = 0.0f;
	}
	else
	{
		float k = step / remaining;
		pos.x += dx * k;
		pos.y += dy * k;
		pos.z += dz * k;
		remaining -= step;
	}

	RwMatrix mat = m_pEntity->GetMatrix().ToRwMatrix();
	if (m_bNeedRotate)
	{
		float t = m_fDistanceToTargetPoint > 0.001f ? 1.0f - remaining / m_fDistanceToTargetPoint : 1.0f;
		if (t < 0.0f) t = 0.0f;
		if (t > 1.0f || done) t = 1.0f;
		CQuaternion quat;
		quat.Slerp(&m_quatStart, &m_quatTarget, t);
		quat.Normalize();
		quat.GetMatrix(&mat);
	}
	mat.pos.x = pos.x;
	mat.pos.y = pos.y;
	mat.pos.z = pos.z;

	// CPhysical::Remove
	((void (*)(CEntityGTA*))(*(uintptr_t*)( *(uintptr*)(m_pEntity) + (VER_x32 ? 0x10 : 0x10*2) )))(m_pEntity);
	m_pEntity->SetMatrix((CMatrix&)mat);
	m_pEntity->UpdateRW();
	m_pEntity->UpdateRwFrame();
	m_pEntity->Add();

	if (done) m_byteMoving &= ~1;
}
