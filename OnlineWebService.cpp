#include "OnlineWebService.h"
#include <QWebSocket>
#include <QJsonDocument>
#include <QDir>
#include <QFile>
#include <QCoreApplication>


OnlineWebService::OnlineWebService(QObject* parent)
    : QObject(parent)
    , m_server(new QWebSocketServer("DowStatsOnlineService", QWebSocketServer::NonSecureMode, this))
{
    loadModUiNames();
    loadLastUniquePlayers();

    connect(m_server, &QWebSocketServer::newConnection, this, &OnlineWebService::onNewConnection);
    connect(m_server, &QWebSocketServer::closed, this, &OnlineWebService::onClosed);
    connect(&m_checkTimeTimer, &QTimer::timeout, this, &OnlineWebService::checkClientsPingTime, Qt::QueuedConnection);
    connect(&m_lastUniquePlayersTimer, &QTimer::timeout, this, &OnlineWebService::updateLastUniquePlayers, Qt::QueuedConnection);

    m_checkTimeTimer.setInterval(1000);
    m_lastUniquePlayersTimer.setInterval(60000);

    m_checkTimeTimer.start();
    m_lastUniquePlayersTimer.start();

    m_server->listen(QHostAddress::Any, 50790);
    qDebug() << "Service started.";
}

OnlineWebService::~OnlineWebService()
{
    m_server->close();
}

void OnlineWebService::sendPingResponse(QWebSocket *clientSocket, QJsonObject *jsonObject)
{
    if (!clientSocket)
        return;

    auto& client = m_clientsMap[clientSocket];

    if (!jsonObject->value("data").isObject())
        return;

    QJsonObject jsonDataObject = jsonObject->value("data").toObject();

    if (client.steamId != jsonDataObject.value("steam_id").toString())
    {
        m_clientsBySteamIdMap.remove(client.steamId);
        client.steamId = jsonDataObject.value("steam_id").toString();
        m_clientsBySteamIdMap.insert(client.steamId, &client);

        if(!m_lastDayPlayersOnlineMap.contains(client.steamId))
            m_lastDayPlayersOnlineMap.insert(client.steamId, QDateTime::currentDateTimeUtc());

        if(!m_lastMonthPlayersOnlineMap.contains(client.steamId))
            m_lastMonthPlayersOnlineMap.insert(client.steamId, QDateTime::currentDateTimeUtc());

        if(!m_lastYearPlayersOnlineMap.contains(client.steamId))
            m_lastYearPlayersOnlineMap.insert(client.steamId, QDateTime::currentDateTimeUtc());

        if(!m_allTimesPlayersOnlineMap.contains(client.steamId))
            m_allTimesPlayersOnlineMap.insert(client.steamId, QDateTime::currentDateTimeUtc());
    }

    if (client.currentMod != jsonDataObject.value("current_mod").toString())
    {
        if (!client.currentMod.isEmpty())
            m_onlineModsCounterMap[client.currentMod]--;

        if (m_onlineModsCounterMap[client.currentMod] <= 0)
            m_onlineModsCounterMap.remove(client.currentMod);

        client.currentMod = jsonDataObject.value("current_mod").toString();


        if (m_onlineModsCounterMap.contains(client.currentMod))
            m_onlineModsCounterMap[client.currentMod]++;
        else
            m_onlineModsCounterMap.insert(client.currentMod, 1);

        updateModsOnlineCountJson();
    }

    if (client.isRanked != jsonDataObject.value("ranked_state").toBool())
        client.isRanked = jsonDataObject.value("ranked_state").toBool();

    client.pingTime = 0;

    QJsonObject messageObject;
    messageObject.insert("op", PingResponse);

    QJsonDocument message;
    message.setObject(messageObject);

    client.webSocket->sendTextMessage(message.toJson().replace('\n',""));

    qDebug() << "SEND PING RESPONSE";
}

void OnlineWebService::sendPlyersRankedStateResponse(QWebSocket *clientSocket, QJsonObject *jsonObject)
{
    if (!clientSocket)
        return;

    auto client = m_clientsMap[clientSocket];

    if (!jsonObject->value("data").isObject())
        return;

    auto dataJsonObject = jsonObject->value("data").toObject();

    if (!dataJsonObject.value("players").isArray())
        return;

    auto playersJsonArray = dataJsonObject.value("players").toArray();

    QJsonArray playersStateJsonArray;

    for(const auto &item : std::as_const(playersJsonArray))
    {
        QJsonObject playerStateJsonObject;

        if (!m_clientsBySteamIdMap.contains(item.toString()))
        {
            playerStateJsonObject.insert("steam_id", item.toString());
            playerStateJsonObject.insert("is_ranked", true);
            playerStateJsonObject.insert("is_online", false);
            playersStateJsonArray.append(playerStateJsonObject);
            continue;
        }

        auto client = m_clientsBySteamIdMap[item.toString()];

        playerStateJsonObject.insert("steam_id", client->steamId);
        playerStateJsonObject.insert("is_ranked", client->isRanked);
        playerStateJsonObject.insert("is_online", true);

        playersStateJsonArray.append(playerStateJsonObject);
    }

    QJsonObject returnedDataJsonObject;
    returnedDataJsonObject.insert("players_state", playersStateJsonArray);

    QJsonObject messageObject;
    messageObject.insert("op", PlyersStateResponse);
    messageObject.insert("data", returnedDataJsonObject);

    QJsonDocument message;
    message.setObject(messageObject);

    client.webSocket->sendTextMessage(message.toJson().replace('\n',""));

    qDebug() << "SEND PLAYERS RANKED STATE RESPONSE";
}

void OnlineWebService::sendUniquePlayersOnlineStatisticResponse(QWebSocket *clientSocket, QJsonObject *jsonObject)
{
    if (!clientSocket)
        return;

    QJsonObject uniquePlayersOnlineStatistic;

    uniquePlayersOnlineStatistic.insert("last_day", m_lastDayPlayersOnlineMap.count());
    uniquePlayersOnlineStatistic.insert("last_month", m_lastMonthPlayersOnlineMap.count());
    uniquePlayersOnlineStatistic.insert("last_year", m_lastYearPlayersOnlineMap.count());
    uniquePlayersOnlineStatistic.insert("all_times", m_allTimesPlayersOnlineMap.count());

    QJsonObject returnedDataJsonObject;
    returnedDataJsonObject.insert("unique_players_online_statistic", uniquePlayersOnlineStatistic);

    QJsonObject messageObject;
    messageObject.insert("op", UniquePlayersOnlineStatisticResponse);
    messageObject.insert("data", returnedDataJsonObject);

    QJsonDocument message;
    message.setObject(messageObject);

    clientSocket->sendTextMessage(message.toJson().replace('\n',""));

    qDebug() << "SEND UNIQUE PLAYERS ONLINE STATISTIC RESPONSE";
}

void OnlineWebService::sendModsOnlineCountResponse(QWebSocket *clientSocket, QJsonObject *jsonObject)
{
    if (!clientSocket)
        return;

    QJsonObject messageObject;
    messageObject.insert("op", ModsOnlineCountResponse);
    messageObject.insert("data", m_modsOnlineCountJson);

    QJsonDocument message;
    message.setObject(messageObject);

    clientSocket->sendTextMessage(message.toJson().replace('\n',""));

    qDebug() << "SEND MODS ONLINE COUNT RESPONSE";
}

void OnlineWebService::updateModsOnlineCountJson()
{
    QJsonArray modsOnlineCount;
    int otherModsOnlineCount = 0;

    for (auto it = m_modUiNames.cbegin(); it != m_modUiNames.cend(); ++it)
    {
        QJsonObject modObject;

        modObject.insert("tech_name", it.value().techName);
        modObject.insert("ui_name", it.value().uiName);
        modObject.insert("order", it.value().order);

        if (m_onlineModsCounterMap.contains(it.key()))
            modObject.insert("online_count", m_onlineModsCounterMap[it.key()]);
        else
            modObject.insert("online_count", 0);

        modsOnlineCount.append(std::move(modObject));
    }


    for (auto it = m_onlineModsCounterMap.cbegin(); it != m_onlineModsCounterMap.cend(); ++it)
    {
        if (!m_modUiNames.contains(it.key()))
            otherModsOnlineCount += m_onlineModsCounterMap[it.key()];
    }

    QJsonObject modObject;

    modObject.insert("tech_name", "other_mods");
    modObject.insert("ui_name", "Other mods");
    modObject.insert("online_count", otherModsOnlineCount);
    modObject.insert("order", m_modUiNames.count());

    modsOnlineCount.append(std::move(modObject));

    QJsonObject returnedDataJsonObject;
    returnedDataJsonObject.insert("mods_online_count", modsOnlineCount);

    m_modsOnlineCountJson = returnedDataJsonObject;
}

void OnlineWebService::loadModUiNames()
{
    QString filePath = QCoreApplication::applicationDirPath() + "/config.json";

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "File not oppened:" << filePath;
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);

    if (doc.isNull()) {
        qWarning() << "JSON parse error" << error.errorString();
        return;
    }

    if (!doc.isArray())
        return;

    auto modsArray = doc.array();

    for(const auto &item : std::as_const(modsArray))
    {
        if (!item.isObject())
            continue;

        auto modObject = item.toObject();

        ModUiName newModUiName;
        newModUiName.techName = modObject.value("tech_name").toString();
        newModUiName.uiName = modObject.value("ui_name").toString();
        newModUiName.order = modObject.value("order").toInt();

        m_modUiNames.insert(newModUiName.techName, newModUiName);
    }
}

void OnlineWebService::updateLastUniquePlayers()
{
    QDateTime curentDate = QDateTime::currentDateTimeUtc();

    if (!(curentDate.time().hour() == 0 && curentDate.time().minute() == 0))
        return;

    for (auto it = m_lastDayPlayersOnlineMap.begin(); it != m_lastDayPlayersOnlineMap.end();)
    {
        if (curentDate.msecsTo(it.value()) > 86400000)
            m_lastDayPlayersOnlineMap.erase(it);
        else
            it++;
    }

    for (auto it = m_lastMonthPlayersOnlineMap.begin(); it != m_lastMonthPlayersOnlineMap.end();)
    {
        if (curentDate.msecsTo(it.value()) > 2678400000)
            m_lastMonthPlayersOnlineMap.erase(it);
        else
            it++;
    }

    for (auto it = m_lastYearPlayersOnlineMap.begin(); it != m_lastYearPlayersOnlineMap.end();)
    {
        if (curentDate.msecsTo(it.value()) > 31536000000)
            m_lastYearPlayersOnlineMap.erase(it);
        else
            it++;
    }

    saveLastUniquePlayers();
}

void OnlineWebService::saveLastUniquePlayers()
{
    QString filePath = QCoreApplication::applicationDirPath() + "/UniquePlayers.json";

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qWarning() << "OnlineWebService::saveLastUniquePlayers() -- Could not open file for writing:" << filePath;
        return;
    }

    auto serialize = [](const QMap<QString, QDateTime>& playersMap) -> QJsonArray {
        QJsonArray array;
        for (auto it = playersMap.begin(); it != playersMap.end(); ++it) {
            QJsonObject playerObj;
            playerObj.insert("steam_id", it.key());
            playerObj.insert("date_time", it.value().toString(Qt::DateFormat::ISODate));
            array.append(playerObj);
        }
        return array;
    };

    QJsonObject rootObject;
    rootObject.insert("last_day",   serialize(m_lastDayPlayersOnlineMap));
    rootObject.insert("last_month", serialize(m_lastMonthPlayersOnlineMap));
    rootObject.insert("last_year",  serialize(m_lastYearPlayersOnlineMap));
    rootObject.insert("all_time",   serialize(m_allTimesPlayersOnlineMap));

    QJsonDocument doc(rootObject);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    qDebug() << "OnlineWebService::saveLastUniquePlayers() -- Data saved to" << filePath;
}

void OnlineWebService::loadLastUniquePlayers()
{
    QString filePath = QCoreApplication::applicationDirPath() + "/UniquePlayers.json";

    QFile file(filePath);
    if (!file.open(QIODevice::ReadWrite | QIODevice::Text)) {
        qWarning() << "OnlineWebService::loadLastUniquePlayers() -- File not oppened:" << filePath;
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);

    if (doc.isNull()) {
        qWarning() << "OnlineWebService::loadLastUniquePlayers() -- JSON parse error" << error.errorString();
        return;
    }

    if (!doc.isObject())
        return;

    auto docObject = doc.object();

    auto lastDayArray = docObject.value("last_day").toArray();
    auto lastMonthArray = docObject.value("last_month").toArray();
    auto lastYearArray = docObject.value("last_year").toArray();
    auto allTimeArray = docObject.value("all_time").toArray();

    auto parse = [](QJsonArray* playersArray, QMap<QString, QDateTime>* playersMap)
    {
        for(const auto &item : std::as_const(*playersArray))
        {
            auto itemObject = item.toObject();
            playersMap->insert(itemObject.value("steam_id").toString(), QDateTime::fromString(itemObject.value("date_time").toString(), Qt::DateFormat::ISODate));
        }
    };

    parse(&lastDayArray, &m_lastDayPlayersOnlineMap);
    parse(&lastMonthArray, &m_lastMonthPlayersOnlineMap);
    parse(&lastYearArray, &m_lastYearPlayersOnlineMap);
    parse(&allTimeArray, &m_allTimesPlayersOnlineMap);
}

void OnlineWebService::onNewConnection()
{
    Client newClient;
    newClient.webSocket = m_server->nextPendingConnection();

    connect(newClient.webSocket, &QWebSocket::textMessageReceived, this, &OnlineWebService::onMessageReceived);
    connect(newClient.webSocket, &QWebSocket::disconnected, this, &OnlineWebService::onClientDisconnectd);

    m_clientsMap.insert(newClient.webSocket, std::move(newClient));
    qDebug() << "CLIENT CONNECTED";
}

void OnlineWebService::onClientDisconnectd()
{
    qDebug() << "ON DISCONNECTED";
    QWebSocket *disconnectedClient = qobject_cast<QWebSocket *>(sender());

    if (disconnectedClient) {
        QString currentMod = m_clientsMap[disconnectedClient].currentMod;

        m_onlineModsCounterMap[currentMod]--;

        if (m_onlineModsCounterMap[currentMod] <= 0)
            m_onlineModsCounterMap.remove(currentMod);

        updateModsOnlineCountJson();

        m_clientsBySteamIdMap.remove(m_clientsMap[disconnectedClient].steamId);

        disconnectedClient->close();
        m_clientsMap.remove(disconnectedClient);

        disconnectedClient->deleteLater();

        qDebug() << "CLIENT DISCONNECTED";
    }
}

void OnlineWebService::onMessageReceived(const QString &message)
{
    QWebSocket *clientSocket = qobject_cast<QWebSocket *>(sender());

    if (!clientSocket)
        return;

    QJsonDocument jsonDocument = QJsonDocument::fromJson(message.toUtf8());

    if (!jsonDocument.isObject())
        return;

    QJsonObject jsonObject = jsonDocument.object();

    int opCode = jsonObject.value("op").toInt();

    switch (opCode){
        case PingRequest: sendPingResponse(clientSocket, &jsonObject); break;
        case PlyersStateRequest: sendPlyersRankedStateResponse(clientSocket, &jsonObject); break;
        case UniquePlayersOnlineStatisticRequest: sendUniquePlayersOnlineStatisticResponse(clientSocket, &jsonObject); break;
        case ModsOnlineCountRequest: sendModsOnlineCountResponse(clientSocket, &jsonObject); break;
    }
}

void OnlineWebService::onClosed()
{
    for (auto& item : m_clientsMap)
    {
        if (item.webSocket)
        {
            item.webSocket->disconnect();
            item.webSocket->deleteLater();
        }
    }
}

void OnlineWebService::checkClientsPingTime()
{
    QList<QWebSocket*> clientsForClose;

    for(auto& item: m_clientsMap)
    {
        item.pingTime++;

        if (item.pingTime > 45)
        {
            clientsForClose.append(item.webSocket);

            qDebug() << "PING TIMEOUT";
        }
    }

    for (auto& item : clientsForClose)
    {
        auto& client = m_clientsMap[item];

        if (client.webSocket)
            client.webSocket->close();
        else
        {
            m_onlineModsCounterMap[client.currentMod]--;

            if (m_onlineModsCounterMap[client.currentMod] <= 0)
                m_onlineModsCounterMap.remove(client.currentMod);

            m_clientsMap.remove(item);
        }
    }

}
