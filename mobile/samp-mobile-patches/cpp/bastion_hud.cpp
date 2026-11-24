
// ---------------------------------------------------------------------------
// Bastion: own HUD panel (assets/ui/index.html, BastionUI.hud). When the server
// sends it (dialog "[hud]", see netrpc.cpp), the stock health/armour bars and the
// money line of CWidgetPlayerInfo are not drawn. The breath bar goes to the page
// too (BastionUI.breath via SAMP.bastionBreath). Weapon icon, ammo and wanted
// stars stay. The widget keeps its touch area (weapon switching).
// ---------------------------------------------------------------------------
volatile bool g_bastionHud = false;
static bool s_bastionInPlayerInfo = false;

static bool s_bastionBreathDrawn = false;
static int s_bastionBreathPct = -1;
static int s_bastionBreathSent = -1;

static void BastionSendBreath(int pct)
{
    if (pct == s_bastionBreathSent) return;
    s_bastionBreathSent = pct;
    if (pJavaWrapper) pJavaWrapper->SetBreath(pct);
}

static void (*BastionPlayerInfoDraw)(uintptr_t* thiz);
static void BastionPlayerInfoDraw_hook(uintptr_t* thiz)
{
    s_bastionInPlayerInfo = true;
    s_bastionBreathDrawn = false;
    BastionPlayerInfoDraw(thiz);
    s_bastionInPlayerInfo = false;
    // The game draws the breath bar while under water or until the air is back
    BastionSendBreath(g_bastionHud && s_bastionBreathDrawn ? s_bastionBreathPct : -1);
}

// Breath bar: same maths as the game (m_fBreath / CStats::GetFatAndMuscleModifier(STAT_MOD_AIR_IN_LUNG))
static float (*BastionAirInLung)(int stat) = nullptr;
static void (*BastionBreathBar)(uintptr_t* thiz, int player, float alpha);
static void BastionBreathBar_hook(uintptr_t* thiz, int player, float alpha)
{
    if (!g_bastionHud) { BastionBreathBar(thiz, player, alpha); return; }
    CPlayerPedGta* ped = FindPlayerPed(player);
    if (!ped || !ped->m_pPlayerData || !BastionAirInLung) { BastionBreathBar(thiz, player, alpha); return; }
    float max = BastionAirInLung(8);
    int pct = max > 0.0f ? (int)(ped->m_pPlayerData->m_fBreath * 100.0f / max + 0.5f) : 100;
    s_bastionBreathPct = pct < 0 ? 0 : pct > 100 ? 100 : pct;
    s_bastionBreathDrawn = true;
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
    BastionAirInLung = (float (*)(int))CHook::getSym("_ZN6CStats23GetFatAndMuscleModifierE17eStatModAbilities");
    CHook::InlineHook("_ZN17CWidgetPlayerInfo15RenderBreathBarEif", &BastionBreathBar_hook, &BastionBreathBar);
    CHook::InlineHook("_ZN5CFont11PrintStringEffPs", &BastionPrintString_hook, &BastionPrintString);
}
