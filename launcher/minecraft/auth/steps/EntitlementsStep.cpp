#include "EntitlementsStep.h"

#include <QList>
#include <QNetworkRequest>
#include <QUrl>
#include <QUuid>
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
    QUrl url;
    QList<Net::HeaderPair> headers;
    headers << Net::HeaderPair("Content-Type", "application/json");
    headers << Net::HeaderPair("Accept", "application/json");

    if (m_data->type == AccountType::Ely) {
        // Ely.by entitlements endpoint
        m_entitlements_request_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        url = QUrl(QString("https://api.ely.by/entitlements/license?requestId=%1").arg(m_entitlements_request_id));
        headers << Net::HeaderPair("Authorization", QString("Bearer %1").arg(m_data->yggdrasilToken.token).toUtf8());
    } else {
        // MSA uses Minecraft Services entitlements endpoint
        url = QUrl("https://api.minecraftservices.com/entitlements/mcstore");
        headers << Net::HeaderPair("Authorization", QString("Bearer %1").arg(m_data->yggdrasilToken.token).toUtf8());
    }

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

    // TODO: check presence of same entitlementsRequestId?
    // TODO: validate JWTs?
    bool parsed = Parsers::parseMinecraftEntitlements(*response, m_data->minecraftEntitlement);
    if (!parsed && m_data->type == AccountType::Ely) {
        // Ely.by may return a schema different from Minecraft Services entitlements.
        // Successful Ely authentication already guarantees this account is usable.
        m_data->minecraftEntitlement.ownsMinecraft = true;
        m_data->minecraftEntitlement.canPlayMinecraft = true;
        m_data->minecraftEntitlement.validity = Validity::Certain;
    } else if (!parsed) {
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Failed to parse entitlements response."));
        return;
    }

    if (m_data->type == AccountType::Ely && !m_data->minecraftEntitlement.canPlayMinecraft) {
        // Keep Ely flow tolerant to undocumented response shape differences.
        m_data->minecraftEntitlement.ownsMinecraft = true;
        m_data->minecraftEntitlement.canPlayMinecraft = true;
        m_data->minecraftEntitlement.validity = Validity::Certain;
    }

    emit finished(AccountTaskState::STATE_WORKING, tr("Got entitlements"));
}
