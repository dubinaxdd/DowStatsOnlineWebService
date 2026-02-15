#ifndef ONLINEWEBSERVICE_H
#define ONLINEWEBSERVICE_H
#include <QMap>
#include <QString>
#include <QWebSocketServer>
#include <QObject>
#include <QTimer>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>

struct Client
{
    QWebSocket* webSocket = nullptr;
    quint32 pingTime = 0;
    QString steamId = "";
    bool isRanked = true;
    QString currentMod = "";
};

struct ModUiName{
    QString techName = "";
    QString uiName = "";
    int order = 0;
};


class OnlineWebService : public QObject
{
    Q_OBJECT
public:
    explicit OnlineWebService(QObject* parent = nullptr);
    ~OnlineWebService();

    enum OpCode : int
    {
        PingRequest = 0,
        PingResponse,
        PlyersStateRequest,
        PlyersStateResponse,
        UniquePlayersOnlineStatisticRequest,
        UniquePlayersOnlineStatisticResponse,
        ModsOnlineCountRequest,
        ModsOnlineCountResponse
    };

private:
    void sendPingResponse(QWebSocket *clientSocket, QJsonObject *jsonObject);
    void sendPlyersRankedStateResponse(QWebSocket *clientSocket, QJsonObject *jsonObject);
    void sendUniquePlayersOnlineStatisticResponse(QWebSocket *clientSocket, QJsonObject *jsonObject);
    void sendModsOnlineCountResponse(QWebSocket *clientSocket, QJsonObject *jsonObject);

    void updateModsOnlineCountJson();
    void loadModUiNames();


private slots:
    void onNewConnection();
    void onClientDisconnectd();
    void onMessageReceived(const QString &message);
    void onClosed();
    void checkClientsPingTime();
    void updateLastUniquePlayers();

    void saveLastUniquePlayers();
    void loadLastUniquePlayers();

private:
    QWebSocketServer* m_server;
    QMap<QWebSocket*, Client> m_clientsMap;
    QMap<QString, Client*> m_clientsBySteamIdMap;
    QMap<QString, int> m_onlineModsCounterMap;

    QMap<QString, QDateTime> m_lastDayPlayersOnlineMap;
    QMap<QString, QDateTime> m_lastMonthPlayersOnlineMap;
    QMap<QString, QDateTime> m_lastYearPlayersOnlineMap;
    QMap<QString, QDateTime> m_allTimesPlayersOnlineMap;

    QMap<QString, ModUiName> m_modUiNames;

    QJsonObject m_modsOnlineCountJson;
    QTimer m_checkTimeTimer;
    QTimer m_lastUniquePlayersTimer;
};

#endif // ONLINEWEBSERVICE_H
