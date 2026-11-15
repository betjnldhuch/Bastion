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
	// Then: no-camera-col, attached vehicle/object, materials. Attachments and
	// materials are not supported by this client yet: the object keeps its own textures.

	if (ObjectID >= MAX_OBJECTS) return;
	CObjectPool* pObjectPool = pNetGame->GetObjectPool();
	if (!pObjectPool) return;
	pObjectPool->New(ObjectID, iModel, vecPos, vecRot, fDrawDistance);
	iTotalObjects++;
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
