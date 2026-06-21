// SPDX-License-Identifier: GPL-3.0-only
#include "YggdrasilLoginDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

#include "minecraft/auth/MinecraftAccount.h"

class YggdrasilLoginDialog::Impl {
   public:
    QLineEdit* username;
    QLineEdit* password;
    QLineEdit* server;
    QDialogButtonBox* box;
};

YggdrasilLoginDialog::YggdrasilLoginDialog(QWidget* parent) : QDialog(parent), m(new Impl())
{
    setWindowTitle(tr("Login (Yggdrasil-compatible)"));
    auto layout = new QVBoxLayout(this);
    auto form = new QFormLayout();
    m->username = new QLineEdit(this);
    m->password = new QLineEdit(this);
    m->password->setEchoMode(QLineEdit::Password);
    m->server = new QLineEdit(this);
    m->server->setPlaceholderText(tr("Optional: custom auth server base URL (e.g. https://ely.by/auth)"));
    form->addRow(new QLabel(tr("Username:")), m->username);
    form->addRow(new QLabel(tr("Password:")), m->password);
    form->addRow(new QLabel(tr("Auth server:")), m->server);
    layout->addLayout(form);
    m->box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(m->box);
    connect(m->box, &QDialogButtonBox::accepted, this, &YggdrasilLoginDialog::onOk);
    connect(m->box, &QDialogButtonBox::rejected, this, &YggdrasilLoginDialog::reject);
}

YggdrasilLoginDialog::~YggdrasilLoginDialog()
{
    delete m;
}

void YggdrasilLoginDialog::onOk()
{
    if (m->username->text().isEmpty()) {
        // quick validation
        return;
    }
    accept();
}

MinecraftAccountPtr YggdrasilLoginDialog::newAccount([[maybe_unused]] QWidget* parent)
{
    YggdrasilLoginDialog dlg(parent);
    if (dlg.exec() != QDialog::Accepted) {
        return nullptr;
    }

    auto account = MinecraftAccount::createBlankMSA();
    // set type to Ely
    account->accountData()->type = AccountType::Ely;
    // store credentials in yggdrasilToken.extra temporarily; password will be cleared after auth
    account->accountData()->yggdrasilToken.extra["username"] = dlg.m->username->text();
    account->accountData()->yggdrasilToken.extra["password"] = dlg.m->password->text();
    if (!dlg.m->server->text().isEmpty()) {
        account->accountData()->yggdrasilToken.extra["authServer"] = dlg.m->server->text();
    }
    return account;
}
