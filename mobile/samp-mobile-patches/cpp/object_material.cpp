void CObject::SetMaterial(int iModel, int iMaterialIndex, char* txdname, char* texturename, uint32_t dwColor)
{
	// Bastion: the phone keeps textures in texdb, so the texture is found by name
	// (GTA ones and ours, bst_*, from bastion_textures.cpp); txd/model are not needed.
	// Textures are shared and never destroyed here (upstream destroyed texdb textures).
	if (iMaterialIndex < 0 || iMaterialIndex >= 16) return;
	RwTexture* tex = nullptr;
	if (texturename && *texturename && strcasecmp(texturename, "none"))
	{
		tex = (RwTexture*)LoadTexture(texturename);
		if (tex) tex->refCount++;
	}
	m_MaterialTexture[iMaterialIndex] = (uintptr_t)tex;
	m_dwMaterialColor[iMaterialIndex] = dwColor;
	m_iMaterialType[iMaterialIndex] = MATERIAL_TYPE_MATERIAL;
	if (tex || dwColor) m_bHasMaterial = true;
	if (m_bHasMaterial && m_pEntity) g_BastionMaterialObjects[m_pEntity] = this;
}

