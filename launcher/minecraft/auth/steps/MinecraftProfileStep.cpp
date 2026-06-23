#include "MinecraftProfileStep.h"

#include <QNetworkRequest>

#include "Application.h"
#include "minecraft/auth/Parsers.h"
#include "net/NetUtils.h"
#include "net/RawHeaderProxy.h"

MinecraftProfileStep::MinecraftProfileStep(AccountData* data) : AuthStep(data) {}

QString MinecraftProfileStep::describe()
{
    return tr("Fetching the Minecraft profile.");
}

void MinecraftProfileStep::perform()
{
    QUrl url;
    QList<Net::HeaderPair> headers;
    headers << Net::HeaderPair("Accept", "application/json");

    if (m_data->type == AccountType::Ely) {
        // Ely.by profile data is available through session profile endpoint by UUID.
        QString profileId = m_data->minecraftProfile.id;
        profileId.remove('-');
        if (profileId.isEmpty()) {
            m_data->minecraftProfile = MinecraftProfile();
            emit finished(AccountTaskState::STATE_WORKING, tr("Account has no Minecraft profile."));
            return;
        }
        QString profileUrl = QString("https://authserver.ely.by/session/profile/%1").arg(profileId);
        url = QUrl(profileUrl);
    } else {
        // MSA and other accounts use Minecraft Services API
        url = QUrl("https://api.minecraftservices.com/minecraft/profile");
        headers << Net::HeaderPair("Content-Type", "application/json");
        headers << Net::HeaderPair("Authorization", QString("Bearer %1").arg(m_data->yggdrasilToken.token).toUtf8());
    }

    auto [request, response] = Net::Download::makeByteArray(url);
    m_request = request;
    m_request->addHeaderProxy(std::make_unique<Net::RawHeaderProxy>(headers));
    m_request->enableAutoRetry(true);

    m_task.reset(new NetJob("MinecraftProfileStep", APPLICATION->network()));
    m_task->setAskRetry(false);
    m_task->addNetAction(m_request);

    connect(m_task.get(), &Task::finished, this, [this, response] { onRequestDone(response); });

    m_task->start();
}

void MinecraftProfileStep::onRequestDone(QByteArray* response)
{
    // Handle HTTP 204 No Content (Ely.by returns this when profile not found)
    if (m_request->replyStatusCode() == 204) {
        // NOTE: Succeed even if we do not have a profile. This is a valid account state.
        emit finished(AccountTaskState::STATE_WORKING, tr("Account has no Minecraft profile."));
        return;
    }
    if (m_request->error() == QNetworkReply::ContentNotFoundError) {
        // NOTE: Succeed even if we do not have a profile. This is a valid account state.
        m_data->minecraftProfile = MinecraftProfile();
        emit finished(AccountTaskState::STATE_WORKING, tr("Account has no Minecraft profile."));
        return;
    }
    if (m_request->error() != QNetworkReply::NoError) {
        qWarning() << "Error getting profile:";
        qWarning() << " HTTP Status       :" << m_request->replyStatusCode();
        qWarning() << " Internal error no.:" << m_request->error();
        qWarning() << " Error string      :" << m_request->errorString();

        qWarning() << " Response:";
        qWarning() << QString::fromUtf8(*response);

        if (Net::isApplicationError(m_request->error())) {
            emit finished(AccountTaskState::STATE_FAILED_SOFT,
                          tr("Minecraft Java profile acquisition failed: %1").arg(m_request->errorString()));
        } else {
            emit finished(AccountTaskState::STATE_OFFLINE,
                          tr("Minecraft Java profile acquisition failed: %1").arg(m_request->errorString()));
        }
        return;
    }
    // Use appropriate parser based on account type
    bool parsed = false;
    if (m_data->type == AccountType::Ely) {
        parsed = Parsers::parseMinecraftProfileMojang(*response, m_data->minecraftProfile);
    } else {
        parsed = Parsers::parseMinecraftProfile(*response, m_data->minecraftProfile);
    }
    if (!parsed) {
        m_data->minecraftProfile = MinecraftProfile();
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Minecraft Java profile response could not be parsed"));
        return;
    }

    emit finished(AccountTaskState::STATE_WORKING, tr("Got Minecraft profile"));
}
