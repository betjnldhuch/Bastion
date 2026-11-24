// Bastion: one material of SetObjectMaterial / CreateObject (type 1 = texture).
// Returns false when the rest of the stream can not be read (text material: its
// text is the last field, so nothing follows that we could still use).
static bool BastionReadObjectMaterial(RakNet::BitStream& bs, CObject* pObject)
{
	uint8_t byteType = 0, byteIndex = 0, byteLen = 0;
	uint16_t wModel = 0;
	uint32_t dwColor = 0;
	char txd[256], tex[256];
	if (!bs.Read(byteType) || byteType != 1) return false;
	bs.Read(byteIndex);
	bs.Read(wModel);
	bs.Read(byteLen); bs.Read(txd, byteLen); txd[byteLen] = '\0';
	bs.Read(byteLen); bs.Read(tex, byteLen); tex[byteLen] = '\0';
	if (!bs.Read(dwColor)) return false;
	if (pObject && byteIndex < 16 && strlen(tex) < 32)
		pObject->SetMaterial(wModel, byteIndex, txd, tex, dwColor);
	return true;
}

// Bastion: object RPCs (upstream GTA-2.11 left them empty / commented out).
void ScrCreateObject(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	OBJECTID ObjectID;
	int iModel;
	CVector vecPos;
	CVector vecRot;
	float fDrawDistance;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(ObjectID);
	bsData.Read(iModel);
	bsData.Read(vecPos.x);
	bsData.Read(vecPos.y);
	bsData.Read(vecPos.z);
	bsData.Read(vecRot.x);
	bsData.Read(vecRot.y);
	bsData.Read(vecRot.z);
	bsData.Read(fDrawDistance);
	// Then: no-camera-col, attached vehicle/object, materials. Attachments are not
	// supported by this client yet; material textures/colours are (text is skipped).
	uint8_t byteNoCameraCol = 0;
	uint16_t wAttached1 = 0xFFFF, wAttached2 = 0xFFFF;
	bsData.Read(byteNoCameraCol);
	bsData.Read(wAttached1);
	bsData.Read(wAttached2);
	if (wAttached1 != 0xFFFF || wAttached2 != 0xFFFF)
	{
		float fAttach[6];
		uint8_t byteSyncRot;
		for (int i = 0; i < 6; i++) bsData.Read(fAttach[i]);
		bsData.Read(byteSyncRot);
	}
	uint8_t byteMaterials = 0;
	bsData.Read(byteMaterials);

	if (ObjectID >= MAX_OBJECTS) return;
	CObjectPool* pObjectPool = pNetGame->GetObjectPool();
	if (!pObjectPool) return;
	pObjectPool->New(ObjectID, iModel, vecPos, vecRot, fDrawDistance);
	iTotalObjects++;

	CObject* pObject = pObjectPool->GetAt(ObjectID);
	for (int i = 0; i < byteMaterials; i++)
		if (!BastionReadObjectMaterial(bsData, pObject)) break;
}

void ScrSetObjectPos(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	OBJECTID ObjectID;
	float fX;
	float fY;
	float fZ;
	float fUnused;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(ObjectID);
	bsData.Read(fX);
	bsData.Read(fY);
	bsData.Read(fZ);
	bsData.Read(fUnused);

	CObjectPool* pObjectPool = pNetGame->GetObjectPool();
	if (!pObjectPool) return;

	CObject* pObject = pObjectPool->GetAt(ObjectID);
	if (pObject) {
		pObject->SetPos(fX, fY, fZ);
	}
}

void ScrSetObjectRotation(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	OBJECTID ObjectID;
	CVector vecRot;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(ObjectID);
	bsData.Read(vecRot.x);
	bsData.Read(vecRot.y);
	bsData.Read(vecRot.z);

	CObjectPool* pObjectPool = pNetGame->GetObjectPool();
	if (!pObjectPool) return;

	CObject* pObject = pObjectPool->GetAt(ObjectID);
	if (!pObject) return;

	pObject->InstantRotate(vecRot.x, vecRot.y, vecRot.z);
}

void ScrMoveObject(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	OBJECTID ObjectID;
	float fPad0, fPad1, fPad2;
	float fPosX, fPosY, fPosZ;
	float fSpeed;
	float fRotX, fRotY, fRotZ;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(ObjectID);
	bsData.Read(fPad0);
	bsData.Read(fPad1);
	bsData.Read(fPad2);
	bsData.Read(fPosX);
	bsData.Read(fPosY);
	bsData.Read(fPosZ);
	bsData.Read(fSpeed);
	bsData.Read(fRotX);
	bsData.Read(fRotY);
	bsData.Read(fRotZ);

	CObjectPool* pObjectPool = pNetGame->GetObjectPool();
	if (!pObjectPool) return;
	CObject* pObject = pObjectPool->GetAt(ObjectID);
	if (pObject) {
		pObject->MoveTo(fPosX, fPosY, fPosZ, fSpeed, fRotX, fRotY, fRotZ);
	}
}

void ScrStopObject(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	OBJECTID ObjectID;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(ObjectID);

	CObjectPool* pObjectPool = pNetGame->GetObjectPool();
	if (!pObjectPool) return;
	CObject* pObject = pObjectPool->GetAt(ObjectID);

	if (pObject) {
		pObject->StopMoving();
	}
}

void ScrSetObjectMaterial(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	OBJECTID ObjectID;
	bsData.Read(ObjectID);
	CObjectPool* pObjectPool = pNetGame ? pNetGame->GetObjectPool() : nullptr;
	if (!pObjectPool || ObjectID >= MAX_OBJECTS) return;
	BastionReadObjectMaterial(bsData, pObjectPool->GetAt(ObjectID));
}
