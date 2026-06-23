#include "EntitlementsStep.h"

#include <QList>
#include <QNetworkRequest>
#include <QUrl>
#include <memory>

#include "Application.h"
#include "Logging.h"
#include "minecraft/auth/Parsers.h"
#include "net/Download.h"
#include "net/NetUtils.h"
#include "net/NetJob.h"
#include "net/RawHeaderProxy.h"
#include "tasks/Task.h"

EntitlementsStep::EntitlementsStep(AccountData* data) : AuthStep(data) {}

QString EntitlementsStep::describe()
{
    return tr("Determining game ownership.");
}

void EntitlementsStep::perform()
{
    if (m_data->type == AccountType::Ely) {
        // Ely.by does not expose a Minecraft Services-compatible entitlements endpoint.
        // Ely authentication is enough to continue the launch flow.
        m_data->minecraftEntitlement.ownsMinecraft = true;
        m_data->minecraftEntitlement.canPlayMinecraft = true;
        m_data->minecraftEntitlement.validity = Validity::Certain;
        emit finished(AccountTaskState::STATE_WORKING, tr("Skipped entitlements check for Ely.by"));
        return;
    }

    QUrl url;
    QList<Net::HeaderPair> headers;
    headers << Net::HeaderPair("Content-Type", "application/json");
    headers << Net::HeaderPair("Accept", "application/json");

    // MSA uses Minecraft Services entitlements endpoint.
    url = QUrl("https://api.minecraftservices.com/entitlements/mcstore");
    headers << Net::HeaderPair("Authorization", QString("Bearer %1").arg(m_data->yggdrasilToken.token).toUtf8());

    auto [request, response] = Net::Download::makeByteArray(url);
    m_request = request;
    m_request->addHeaderProxy(std::make_unique<Net::RawHeaderProxy>(headers));
    m_request->enableAutoRetry(true);

    m_task.reset(new NetJob("EntitlementsStep", APPLICATION->network()));
    m_task->setAskRetry(false);
    m_task->addNetAction(m_request);

    connect(m_task.get(), &Task::finished, this, [this, response] { onRequestDone(response); });

    m_task->start();
    qDebug() << "Getting entitlements...";
}

void EntitlementsStep::onRequestDone(QByteArray* response)
{
    qCDebug(authCredentials()) << *response;

    if (m_request->error() != QNetworkReply::NoError) {
        if (Net::isApplicationError(m_request->error())) {
            emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Entitlements request failed: %1").arg(m_request->errorString()));
        } else {
            emit finished(AccountTaskState::STATE_OFFLINE, tr("Entitlements request failed: %1").arg(m_request->errorString()));
        }
        return;
    }

    // TODO: validate JWTs?
    bool parsed = Parsers::parseMinecraftEntitlements(*response, m_data->minecraftEntitlement);
    if (!parsed) {
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Failed to parse entitlements response."));
        return;
    }

    emit finished(AccountTaskState::STATE_WORKING, tr("Got entitlements"));
}
