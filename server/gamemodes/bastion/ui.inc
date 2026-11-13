// =====================================================================
//  ui.inc Ч красивые HTML-окна в клиенте Bastion (пока Android).
//
//   ак это работает: окно отправл€етс€ обычным диалогом, но с заголовком
//  "[ui:»ћя]" и данными в виде JSON в тексте. Ќаш клиент вместо
//  стандартного диалога открывает HTML-окно »ћя (assets/ui/index.html
//  в APK), а ответ игрока приходит обычным OnDialogResponse с тем же
//  dialogid. ѕлагины на сервере не нужны.
//
//   то умеет окна: клиент Bastion добавл€ет "-bui" к версии
//  (GetPlayerVersion, например "0.3.7-bui1"). ѕ -плагин bastion_ui.asi
//  шлЄт скрытую команду /__bui при первом диалоге. ќстальным показываем
//  прежние диалоги Ч провер€й UI_Has(playerid) перед UI_Show.
// =====================================================================

#define UI_CLIENT_TAG   "-bui"

new bool:gUiSupported[MAX_PLAYERS];

// ¬ызываетс€ из OnPlayerConnect (rp.pwn)
stock UI_OnConnect(playerid)
{
	new version[32];
	GetPlayerVersion(playerid, version, sizeof version);
	gUiSupported[playerid] = (strfind(version, UI_CLIENT_TAG) != -1);
	if (gUiSupported[playerid]) printf("[ui] id=%d HTML-окна включены (%s)", playerid, version);
}

// ѕ -клиент с bastion_ui.asi: команда /__bui (из OnPlayerCommandText).
// ќкно входа уже показано обычным диалогом Ч показываем его заново HTML-окном.
stock UI_OnPcHello(playerid)
{
	if (gUiSupported[playerid]) return 1;
	gUiSupported[playerid] = true;
	printf("[ui] id=%d HTML-окна включены (ѕ , bastion_ui.asi)", playerid);
	if (!Player[playerid][pLogged]) Account_ShowAuth(playerid);
	return 1;
}

stock UI_OnDisconnect(playerid)
{
	gUiSupported[playerid] = false;
}

stock bool:UI_Has(playerid)
{
	return gUiSupported[playerid];
}

// ѕоказать HTML-окно. json Ч объект с данными окна, например {"name":"Ivan_Petrov"}.
// style важен дл€ ответа: LIST Ч если окно возвращает пункт (listitem),
// INPUT/PASSWORD Ч если возвращает текст, MSGBOX Ч только кнопку.
stock UI_Show(playerid, dialogid, DIALOG_STYLE:style, const window[], const json[], const button1[], const button2[] = "")
{
	new caption[40];
	format(caption, sizeof caption, "[ui:%s]", window);
	return ShowPlayerDialog(playerid, dialogid, style, caption, "%s", button1, button2, json);
}

// Ёкранирует строку дл€ JSON: кавычки, обратный слэш, перевод строки.
stock UI_JsonEscape(dest[], const src[], size = sizeof dest)
{
	new j = 0;
	for (new i = 0; src[i] != '\0' && j < size - 2; i++)
	{
		switch (src[i])
		{
			case '"', '\\': { dest[j++] = '\\'; dest[j++] = src[i]; }
			case '\n': { dest[j++] = '\\'; dest[j++] = 'n'; }
			case '\r', '\t': dest[j++] = ' ';
			default: dest[j++] = src[i];
		}
	}
	dest[j] = '\0';
}
