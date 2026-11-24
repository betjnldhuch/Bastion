// Bastion: SetObjectMaterial on the phone. The geometry of a model is shared by
// every copy of it, so the textures/colours of this object are put on the
// materials only for its own render call and restored right after it.
#include <unordered_map>
extern std::unordered_map<void*, CObject*> g_BastionMaterialObjects;

struct BastionMatSave { RpMaterial* mat; RwTexture* tex; RwRGBA color; };
struct BastionMatCtx { CObject* obj; int index; int count; BastionMatSave save[64]; };

static RpMaterial* BastionMaterialCB(RpMaterial* mat, void* data)
{
	BastionMatCtx* ctx = (BastionMatCtx*)data;
	int i = ctx->index++;
	if (!mat || i >= 16 || ctx->count >= 64) return mat;
	RwTexture* tex = (RwTexture*)ctx->obj->m_MaterialTexture[i];
	uint32_t argb = ctx->obj->m_dwMaterialColor[i];
	if (!tex && !argb) return mat;
	ctx->save[ctx->count++] = { mat, mat->texture, mat->color };
	if (tex) mat->texture = tex;
	if (argb)
	{
		mat->color.red = (argb >> 16) & 0xFF;
		mat->color.green = (argb >> 8) & 0xFF;
		mat->color.blue = argb & 0xFF;
		mat->color.alpha = (argb >> 24) & 0xFF;
	}
	return mat;
}

static RpAtomic* BastionMaterialAtomicCB(RpAtomic* atomic, void* data)
{
	if (atomic && atomic->geometry) RpGeometryForAllMaterials(atomic->geometry, BastionMaterialCB, data);
	return atomic;
}

void (*CObject_Render)(CObjectGta* thiz);
void CObject_Render_hook(CObjectGta* thiz)
{
	if (!thiz) return;
	BastionMatCtx ctx;
	ctx.count = 0;
	if (!g_BastionMaterialObjects.empty())
	{
		auto it = g_BastionMaterialObjects.find((void*)thiz);
		CObject* pObject = it != g_BastionMaterialObjects.end() ? it->second : nullptr;
		RwObject* rw = (pObject && pObject->m_pEntity) ? (RwObject*)pObject->m_pEntity->m_pRwObject : nullptr;
		if (rw)
		{
			ctx.obj = pObject;
			ctx.index = 0;
			if (rw->type == 1) BastionMaterialAtomicCB((RpAtomic*)rw, &ctx);
			else if (rw->type == 2) RpClumpForAllAtomics((RpClump*)rw, BastionMaterialAtomicCB, &ctx);
		}
	}
	CObject_Render(thiz);
	for (int i = ctx.count - 1; i >= 0; i--)
	{
		ctx.save[i].mat->texture = ctx.save[i].tex;
		ctx.save[i].mat->color = ctx.save[i].color;
	}
}

