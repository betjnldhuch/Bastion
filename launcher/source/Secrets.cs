// =====================================================================
//  Адрес архива игры и ключ закрытой раздачи (cloud/download-worker).
//  В репозитории значения пустые: GitHub Actions подставляет их из
//  секретов BASTION_GAME_URL, BASTION_GAME_MD5, BASTION_DOWNLOAD_KEY
//  при сборке. Не вписывай сюда настоящий ключ и не коммить его.
// =====================================================================
namespace BastionLauncher
{
    static class Secrets
    {
        public const string GameUrl = "";
        public const string GameMd5 = "";
        public const string DownloadKey = "";
    }
}
