// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QDialog>

#include "minecraft/auth/MinecraftAccount.h"

class YggdrasilLoginDialog : public QDialog {
    Q_OBJECT

   public:
    explicit YggdrasilLoginDialog(QWidget* parent = nullptr);
    ~YggdrasilLoginDialog() override;

    static MinecraftAccountPtr newAccount(QWidget* parent = nullptr);

   private slots:
    void onOk();

   private:
    class Impl;
    Impl* m;
};
