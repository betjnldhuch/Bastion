
// ---------------------------------------------------------------------------
// Bastion: own HUD panel (assets/ui/index.html, BastionUI.hud). When the server
// sends it (dialog "[hud]", see netrpc.cpp), the stock health/armour bars and the
// money line of CWidgetPlayerInfo are not drawn. Weapon icon, ammo, breath and
// wanted stars stay. The widget keeps its touch area (weapon switching).
// ---------------------------------------------------------------------------
volatile bool g_bastionHud = false;
static bool s_bastionInPlayerInfo = false;

static void (*BastionPlayerInfoDraw)(uintptr_t* thiz);
static void BastionPlayerInfoDraw_hook(uintptr_t* thiz)
{
    s_bastionInPlayerInfo = true;
    BastionPlayerInfoDraw(thiz);
    s_bastionInPlayerInfo = false;
}

static void (*BastionHealthBar)(uintptr_t* thiz, int player, float alpha);
static void BastionHealthBar_hook(uintptr_t* thiz, int player, float alpha)
{
    if (!g_bastionHud) BastionHealthBar(thiz, player, alpha);
}

static void (*BastionArmorBar)(uintptr_t* thiz, int player, float alpha);
static void BastionArmorBar_hook(uintptr_t* thiz, int player, float alpha)
{
    if (!g_bastionHud) BastionArmorBar(thiz, player, alpha);
}

// Money is "$00001500" / "-$0001500" (GXT chars) printed inside CWidgetPlayerInfo::Draw
static void (*BastionPrintString)(float x, float y, uint16_t* text);
static void BastionPrintString_hook(float x, float y, uint16_t* text)
{
    if (g_bastionHud && s_bastionInPlayerInfo && text && (text[0] == '$' || (text[0] == '-' && text[1] == '$'))) return;
    BastionPrintString(x, y, text);
}

static void BastionInstallHudHooks()
{
    CHook::InlineHook("_ZN17CWidgetPlayerInfo4DrawEv", &BastionPlayerInfoDraw_hook, &BastionPlayerInfoDraw);
    CHook::InlineHook("_ZN17CWidgetPlayerInfo15RenderHealthBarEif", &BastionHealthBar_hook, &BastionHealthBar);
    CHook::InlineHook("_ZN17CWidgetPlayerInfo14RenderArmorBarEif", &BastionArmorBar_hook, &BastionArmorBar);
    CHook::InlineHook("_ZN5CFont11PrintStringEffPs", &BastionPrintString_hook, &BastionPrintString);
}
