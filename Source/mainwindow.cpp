#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDoubleValidator>
#include <QFile>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QLockFile>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSaveFile>
#include <QScrollArea>
#include <QStandardPaths>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QUuid>

namespace {

QLabel *makeLabel(const QString &text, const QString &styleClass = QString())
{
    auto *label = new QLabel(text);
    if (!styleClass.isEmpty())
        label->setProperty("class", styleClass);
    return label;
}

QFrame *makeCard(QWidget *parent = nullptr)
{
    auto *frame = new QFrame(parent);
    frame->setProperty("class", "card");
    frame->setFrameShape(QFrame::NoFrame);
    return frame;
}

QLineEdit *makeInput(const QString &placeholder, bool password = false)
{
    auto *input = new QLineEdit;
    input->setPlaceholderText(placeholder);
    if (password)
        input->setEchoMode(QLineEdit::Password);
    return input;
}

QLineEdit *makeAmountInput()
{
    auto *input = makeInput(QStringLiteral("0.00"));
    auto *validator = new QDoubleValidator(0.0, 1000000000000.0, 2, input);
    validator->setNotation(QDoubleValidator::StandardNotation);
    validator->setLocale(QLocale::c());
    input->setValidator(validator);
    return input;
}

bool readAmount(const QLineEdit *input, double *amount)
{
    const QString text = input->text().trimmed();
    if (text.isEmpty()) {
        *amount = 0.0;
        return true;
    }

    bool valid = false;
    const double value = text.toDouble(&valid);
    if (!valid || value < 0.0 || value > 1000000000000.0)
        return false;
    *amount = value;
    return true;
}

QPushButton *makeButton(const QString &text, const QString &styleClass)
{
    auto *button = new QPushButton(text);
    button->setProperty("class", styleClass);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

void addPageHeading(QVBoxLayout *layout, const QString &title, const QString &subtitle)
{
    layout->addWidget(makeLabel(title, "pageTitle"));
    layout->addWidget(makeLabel(subtitle, "pageSubtitle"));
    layout->addSpacing(18);
}

void clearLayout(QLayout *layout)
{
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (QWidget *widget = item->widget())
            delete widget;
        if (QLayout *childLayout = item->layout())
            clearLayout(childLayout);
        delete item;
    }
}

QString moneyText(double amount)
{
    return QStringLiteral("Rs. %1").arg(amount, 0, 'f', 2);
}

QString accountFilePath()
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/account.json");
}

QString legacyAccountFilePath()
{
    const QString directory =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return directory + QStringLiteral("/account.json");
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle(QStringLiteral("Northstar | Personal Banking"));
    resize(1120, 760);
    setMinimumSize(900, 640);

    setStyleSheet(QStringLiteral(
        "QMainWindow, QWidget#pageRoot { background: #f3f6fa; color: #17243b; }"
        "QWidget { font-family: 'Segoe UI'; font-size: 14px; }"
        "QFrame[class=\"authPanel\"] { background: #10233f; border-radius: 22px; }"
        "QFrame[class=\"authCard\"], QFrame[class=\"card\"] { background: #ffffff; border: 1px solid #e6ebf2; "
        "border-radius: 18px; }"
        "QLabel[class=\"brand\"] { color: #ffffff; font-size: 20px; font-weight: 700; }"
        "QLabel[class=\"authHero\"] { color: #ffffff; font-size: 36px; font-weight: 700; }"
        "QLabel[class=\"authCopy\"] { color: #c4d0e0; font-size: 15px; }"
        "QLabel[class=\"eyebrow\"] { color: #58c4a4; font-size: 11px; font-weight: 700; "
        "letter-spacing: 2px; }"
        "QLabel[class=\"authTitle\"] { color: #17243b; font-size: 28px; font-weight: 700; }"
        "QLabel[class=\"pageTitle\"] { color: #17243b; font-size: 26px; font-weight: 700; }"
        "QLabel[class=\"pageSubtitle\"] { color: #728097; font-size: 14px; }"
        "QLabel[class=\"fieldLabel\"] { color: #45536a; font-size: 12px; font-weight: 600; }"
        "QLabel[class=\"balanceAmount\"] { color: #ffffff; font-size: 36px; font-weight: 700; }"
        "QLabel[class=\"metricValue\"] { color: #17243b; font-size: 28px; font-weight: 700; }"
        "QLabel[class=\"muted\"] { color: #8290a4; font-size: 12px; }"
        "QLabel[class=\"whiteMuted\"] { color: #ced8e7; font-size: 13px; }"
        "QLineEdit { min-height: 42px; padding: 0 12px; color: #17243b; "
        "background: #f8fafc; border: 1px solid #dfe6ef; border-radius: 9px; }"
        "QLineEdit:focus { border: 1px solid #2da987; "
        "background: #ffffff; }"
        "QPushButton { min-height: 42px; border: none; border-radius: 9px; padding: 0 16px; "
        "font-weight: 600; }"
        "QPushButton[class=\"primary\"] { color: #ffffff; background: #19866d; }"
        "QPushButton[class=\"primary\"]:hover { background: #14765f; }"
        "QPushButton[class=\"secondary\"] { color: #19866d; background: #e9f5f1; }"
        "QPushButton[class=\"secondary\"]:hover { background: #dcefe9; }"
        "QPushButton[class=\"link\"] { color: #19866d; background: transparent; padding: 0; }"
        "QPushButton[class=\"link\"]:hover { color: #126a55; }"
        "QWidget#sidebar { background: #10233f; }"
        "QLabel[class=\"sidebarBrand\"] { color: #ffffff; font-size: 20px; font-weight: 700; }"
        "QLabel[class=\"sidebarCaption\"] { color: #91a2ba; font-size: 10px; font-weight: 700; "
        "letter-spacing: 1px; }"
        "QPushButton[class=\"navButton\"] { color: #bdc9d8; background: transparent; text-align: left; "
        "padding-left: 14px; min-height: 44px; }"
        "QPushButton[class=\"navButton\"]:hover { color: #ffffff; background: #1c3659; }"
        "QPushButton[class=\"navButton\"]:checked { color: #ffffff; background: #1d765f; }"
        "QPushButton[class=\"logoutButton\"] { color: #f0b7b7; background: #263b58; }"
        "QPushButton[class=\"logoutButton\"]:hover { color: #ffffff; background: #7d3942; }"
        "QLabel[class=\"headerName\"] { color: #17243b; font-size: 14px; font-weight: 700; }"
        "QLabel[class=\"headerAccount\"] { color: #8290a4; font-size: 12px; }"
        "QLabel[class=\"transactionType\"] { color: #26354c; font-weight: 600; }"
        "QLabel[class=\"positiveAmount\"] { color: #19866d; font-weight: 700; }"
        "QLabel[class=\"negativeAmount\"] { color: #bd5360; font-weight: 700; }"
        "QScrollArea { border: none; background: transparent; }"
        "QScrollArea QWidget { background: transparent; }"
    ));

    pageStack = new QStackedWidget(this);
    setCentralWidget(pageStack);
    pageStack->addWidget(createLoginPage());
    pageStack->addWidget(createSignupPage());

    loadAccount();
}

MainWindow::~MainWindow()
{
    delete ui;
}

QWidget *MainWindow::createLoginPage()
{
    auto *page = new QWidget;
    auto *outer = new QHBoxLayout(page);
    outer->setContentsMargins(34, 34, 34, 34);
    outer->setSpacing(28);

    auto *brandPanel = new QFrame;
    brandPanel->setProperty("class", "authPanel");
    auto *brandLayout = new QVBoxLayout(brandPanel);
    brandLayout->setContentsMargins(42, 42, 42, 42);
    brandLayout->addWidget(makeLabel(QStringLiteral("✦  NORTHSTAR"), "brand"));
    brandLayout->addStretch();
    brandLayout->addWidget(makeLabel(QStringLiteral("PERSONAL BANKING"), "eyebrow"));
    auto *hero = makeLabel(QStringLiteral("Your money,\nbeautifully in balance."), "authHero");
    hero->setWordWrap(true);
    brandLayout->addWidget(hero);
    auto *copy = makeLabel(
        QStringLiteral("A clearer view of your finances, with the essentials always within reach."),
        "authCopy");
    copy->setWordWrap(true);
    brandLayout->addWidget(copy);
    brandLayout->addSpacing(24);
    brandLayout->addWidget(makeLabel(QStringLiteral("Simple. Thoughtful. Yours."), "whiteMuted"));
    brandLayout->addStretch();
    outer->addWidget(brandPanel, 3);

    auto *rightSide = new QWidget;
    auto *rightLayout = new QVBoxLayout(rightSide);
    rightLayout->setContentsMargins(8, 8, 8, 8);
    rightLayout->addStretch();

    auto *card = new QFrame;
    card->setProperty("class", "authCard");
    card->setMaximumWidth(440);
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(34, 34, 34, 30);
    cardLayout->setSpacing(13);
    cardLayout->addWidget(makeLabel(QStringLiteral("WELCOME BACK"), "eyebrow"));
    cardLayout->addWidget(makeLabel(QStringLiteral("Sign in"), "authTitle"));
    cardLayout->addWidget(makeLabel(
        QStringLiteral("Use your account number or customer name."), "pageSubtitle"));
    cardLayout->addSpacing(8);
    cardLayout->addWidget(makeLabel(QStringLiteral("Account number or name"), "fieldLabel"));
    loginIdentity = makeInput(QStringLiteral("e.g. 10002468"));
    cardLayout->addWidget(loginIdentity);
    cardLayout->addWidget(makeLabel(QStringLiteral("Password"), "fieldLabel"));
    loginPassword = makeInput(QStringLiteral("Enter your password"), true);
    cardLayout->addWidget(loginPassword);
    cardLayout->addSpacing(7);
    auto *loginButton = makeButton(QStringLiteral("Sign in  →"), "primary");
    cardLayout->addWidget(loginButton);
    cardLayout->addWidget(makeLabel(QStringLiteral("New to Northstar?"), "muted"),
                          0, Qt::AlignHCenter);
    auto *signupButton = makeButton(QStringLiteral("Create an account"), "link");
    cardLayout->addWidget(signupButton, 0, Qt::AlignHCenter);
    rightLayout->addWidget(card, 0, Qt::AlignHCenter);
    rightLayout->addStretch();
    outer->addWidget(rightSide, 2);

    connect(loginButton, &QPushButton::clicked, this, &MainWindow::login);
    connect(loginPassword, &QLineEdit::returnPressed, this, &MainWindow::login);
    connect(signupButton, &QPushButton::clicked, this, [this] { fadeToPage(1); });
    return page;
}

QWidget *MainWindow::createSignupPage()
{
    auto *page = new QWidget;
    auto *outer = new QHBoxLayout(page);
    outer->setContentsMargins(34, 34, 34, 34);
    outer->setSpacing(28);

    auto *brandPanel = new QFrame;
    brandPanel->setProperty("class", "authPanel");
    auto *brandLayout = new QVBoxLayout(brandPanel);
    brandLayout->setContentsMargins(42, 42, 42, 42);
    brandLayout->addWidget(makeLabel(QStringLiteral("✦  NORTHSTAR"), "brand"));
    brandLayout->addStretch();
    brandLayout->addWidget(makeLabel(QStringLiteral("A BETTER WAY TO BANK"), "eyebrow"));
    auto *hero = makeLabel(QStringLiteral("Start with a\nfresh perspective."), "authHero");
    hero->setWordWrap(true);
    brandLayout->addWidget(hero);
    auto *copy = makeLabel(
        QStringLiteral("Create your personal demo account and bring your everyday banking into one calm, clear space."),
        "authCopy");
    copy->setWordWrap(true);
    brandLayout->addWidget(copy);
    brandLayout->addStretch();
    outer->addWidget(brandPanel, 3);

    auto *rightSide = new QWidget;
    auto *rightLayout = new QVBoxLayout(rightSide);
    rightLayout->setContentsMargins(8, 8, 8, 8);
    rightLayout->addStretch();

    auto *card = new QFrame;
    card->setProperty("class", "authCard");
    card->setMaximumWidth(480);
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(32, 28, 32, 24);
    cardLayout->setSpacing(9);
    cardLayout->addWidget(makeLabel(QStringLiteral("GET STARTED"), "eyebrow"));
    cardLayout->addWidget(makeLabel(QStringLiteral("Create your account"), "authTitle"));
    cardLayout->addWidget(makeLabel(
        QStringLiteral("Your information stays on this device."), "pageSubtitle"));
    cardLayout->addSpacing(3);

    cardLayout->addWidget(makeLabel(QStringLiteral("Customer name"), "fieldLabel"));
    signupName = makeInput(QStringLiteral("Full name"));
    cardLayout->addWidget(signupName);
    cardLayout->addWidget(makeLabel(QStringLiteral("Account number"), "fieldLabel"));
    signupNumber = makeInput(QStringLiteral("Choose an account number"));
    signupNumber->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("[0-9]*")), signupNumber));
    cardLayout->addWidget(signupNumber);
    cardLayout->addWidget(makeLabel(QStringLiteral("Initial balance (Rs.)"), "fieldLabel"));
    signupBalance = makeAmountInput();
    cardLayout->addWidget(signupBalance);

    auto *passwordRow = new QHBoxLayout;
    auto *passwordColumn = new QVBoxLayout;
    passwordColumn->addWidget(makeLabel(QStringLiteral("Password"), "fieldLabel"));
    signupPassword = makeInput(QStringLiteral("At least 8 characters"), true);
    passwordColumn->addWidget(signupPassword);
    auto *confirmColumn = new QVBoxLayout;
    confirmColumn->addWidget(makeLabel(QStringLiteral("Confirm password"), "fieldLabel"));
    signupPasswordConfirm = makeInput(QStringLiteral("Repeat password"), true);
    confirmColumn->addWidget(signupPasswordConfirm);
    passwordRow->addLayout(passwordColumn);
    passwordRow->addSpacing(10);
    passwordRow->addLayout(confirmColumn);
    cardLayout->addLayout(passwordRow);
    cardLayout->addSpacing(5);

    auto *createButton = makeButton(QStringLiteral("Create account  →"), "primary");
    cardLayout->addWidget(createButton);
    auto *backButton = makeButton(QStringLiteral("←  Back to sign in"), "link");
    cardLayout->addWidget(backButton, 0, Qt::AlignHCenter);
    rightLayout->addWidget(card, 0, Qt::AlignHCenter);
    rightLayout->addStretch();
    outer->addWidget(rightSide, 3);

    connect(createButton, &QPushButton::clicked, this, &MainWindow::createAccount);
    connect(backButton, &QPushButton::clicked, this, [this] { fadeToPage(0); });
    signupPasswordConfirm->setToolTip(QStringLiteral("Use at least 8 characters."));
    return page;
}

QWidget *MainWindow::createDashboardPage()
{
    auto *page = new QWidget;
    auto *root = new QHBoxLayout(page);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *sidebar = new QWidget;
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setFixedWidth(228);
    auto *sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(18, 24, 18, 18);
    sideLayout->setSpacing(7);
    sideLayout->addWidget(makeLabel(QStringLiteral("✦  NORTHSTAR"), "sidebarBrand"));
    sideLayout->addWidget(makeLabel(QStringLiteral("PERSONAL BANKING"), "sidebarCaption"));
    sideLayout->addSpacing(26);
    sideLayout->addWidget(makeLabel(QStringLiteral("YOUR BANKING"), "sidebarCaption"));

    const QStringList names = {
        QStringLiteral("Overview"),
        QStringLiteral("Deposit"),
        QStringLiteral("Withdraw"),
        QStringLiteral("Balance"),
        QStringLiteral("Account details"),
        QStringLiteral("Transaction history"),
        QStringLiteral("Password management")
    };
    for (int i = 0; i < names.size(); ++i) {
        auto *button = makeButton(names.at(i), "navButton");
        button->setCheckable(true);
        if (i == Overview)
            button->setChecked(true);
        navigationButtons.append(button);
        sideLayout->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, i] {
            showFeature(static_cast<Feature>(i));
        });
    }
    sideLayout->addStretch();
    auto *logoutButton = makeButton(QStringLiteral("↪  Sign out"), "logoutButton");
    sideLayout->addWidget(logoutButton);
    connect(logoutButton, &QPushButton::clicked, this, &MainWindow::logout);
    root->addWidget(sidebar);

    auto *content = new QWidget;
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(30, 18, 30, 24);
    contentLayout->setSpacing(16);

    auto *header = new QWidget;
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerGreeting = makeLabel(QStringLiteral("Welcome"), "headerName");
    headerAccountNumber = makeLabel(QStringLiteral("Account —"), "headerAccount");
    auto *userInfo = new QVBoxLayout;
    userInfo->setSpacing(2);
    userInfo->addWidget(headerGreeting);
    userInfo->addWidget(headerAccountNumber);
    headerLayout->addStretch();
    headerLayout->addLayout(userInfo);
    contentLayout->addWidget(header);

    featureStack = new QStackedWidget;
    featureStack->addWidget(createOverviewPage());
    featureStack->addWidget(createDepositPage());
    featureStack->addWidget(createWithdrawPage());
    featureStack->addWidget(createBalancePage());
    featureStack->addWidget(createAccountDetailsPage());
    featureStack->addWidget(createHistoryPage());
    featureStack->addWidget(createPasswordPage());
    contentLayout->addWidget(featureStack, 1);
    root->addWidget(content, 1);
    return page;
}

QWidget *MainWindow::createOverviewPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);
    addPageHeading(layout, QStringLiteral("Your overview"),
                   QStringLiteral("A quick look at your account and recent activity."));

    auto *balanceCard = makeCard();
    balanceCard->setStyleSheet(QStringLiteral(
        "QFrame { background: #173c59; border: none; border-radius: 18px; }"));
    auto *balanceLayout = new QVBoxLayout(balanceCard);
    balanceLayout->setContentsMargins(25, 20, 25, 20);
    balanceLayout->addWidget(makeLabel(QStringLiteral("TOTAL AVAILABLE BALANCE"), "whiteMuted"));
    overviewBalance = makeLabel(QStringLiteral("Rs. 0.00"), "balanceAmount");
    balanceLayout->addWidget(overviewBalance);
    balanceLayout->addWidget(makeLabel(QStringLiteral("Your account, at a glance"), "whiteMuted"));
    layout->addWidget(balanceCard);

    auto *quickActions = new QHBoxLayout;
    auto *depositButton = makeButton(QStringLiteral("+  Deposit money"), "primary");
    auto *withdrawButton = makeButton(QStringLiteral("−  Withdraw money"), "secondary");
    auto *historyButton = makeButton(QStringLiteral("View activity"), "secondary");
    quickActions->addWidget(depositButton);
    quickActions->addWidget(withdrawButton);
    quickActions->addWidget(historyButton);
    quickActions->addStretch();
    layout->addLayout(quickActions);

    auto *recentCard = makeCard();
    auto *recentLayout = new QVBoxLayout(recentCard);
    recentLayout->setContentsMargins(20, 17, 20, 14);
    auto *heading = new QHBoxLayout;
    heading->addWidget(makeLabel(QStringLiteral("Recent activity"), "transactionType"));
    heading->addStretch();
    recentLayout->addLayout(heading);
    recentTransactionsContainer = new QWidget;
    recentTransactionsContainer->setLayout(new QVBoxLayout);
    recentTransactionsContainer->layout()->setContentsMargins(0, 8, 0, 0);
    recentTransactionsContainer->layout()->setSpacing(8);
    recentLayout->addWidget(recentTransactionsContainer);
    layout->addWidget(recentCard, 1);

    connect(depositButton, &QPushButton::clicked, this,
            [this] { showFeature(Deposit); });
    connect(withdrawButton, &QPushButton::clicked, this,
            [this] { showFeature(Withdraw); });
    connect(historyButton, &QPushButton::clicked, this,
            [this] { showFeature(History); });
    return page;
}

QWidget *MainWindow::createDepositPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    addPageHeading(layout, QStringLiteral("Deposit money"),
                   QStringLiteral("Add funds to your account."));

    auto *card = makeCard();
    card->setMaximumWidth(620);
    auto *form = new QVBoxLayout(card);
    form->setContentsMargins(26, 24, 26, 24);
    form->setSpacing(14);
    form->addWidget(makeLabel(QStringLiteral("Deposit amount (Rs.)"), "fieldLabel"));
    depositAmount = makeAmountInput();
    form->addWidget(depositAmount);
    form->addWidget(makeLabel(
        QStringLiteral("The updated balance and transaction record will be saved on this device."),
        "muted"));
    auto *submit = makeButton(QStringLiteral("Confirm deposit"), "primary");
    form->addWidget(submit);
    layout->addWidget(card);
    layout->addStretch();
    connect(submit, &QPushButton::clicked, this,
            [this] { performTransaction(true); });
    return page;
}

QWidget *MainWindow::createWithdrawPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    addPageHeading(layout, QStringLiteral("Withdraw money"),
                   QStringLiteral("Move funds out of your account."));

    auto *card = makeCard();
    card->setMaximumWidth(620);
    auto *form = new QVBoxLayout(card);
    form->setContentsMargins(26, 24, 26, 24);
    form->setSpacing(14);
    form->addWidget(makeLabel(QStringLiteral("Withdrawal amount (Rs.)"), "fieldLabel"));
    withdrawAmount = makeAmountInput();
    form->addWidget(withdrawAmount);
    form->addWidget(makeLabel(
        QStringLiteral("Withdrawals cannot exceed your available balance."), "muted"));
    auto *submit = makeButton(QStringLiteral("Confirm withdrawal"), "primary");
    form->addWidget(submit);
    layout->addWidget(card);
    layout->addStretch();
    connect(submit, &QPushButton::clicked, this,
            [this] { performTransaction(false); });
    return page;
}

QWidget *MainWindow::createBalancePage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    addPageHeading(layout, QStringLiteral("Balance inquiry"),
                   QStringLiteral("Your available balance, updated in real time."));

    auto *card = makeCard();
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(26, 25, 26, 25);
    cardLayout->addWidget(makeLabel(QStringLiteral("AVAILABLE BALANCE"), "eyebrow"));
    balanceValue = makeLabel(QStringLiteral("Rs. 0.00"), "metricValue");
    cardLayout->addWidget(balanceValue);
    cardLayout->addWidget(makeLabel(QStringLiteral("Account balance"), "muted"));
    layout->addWidget(card);
    layout->addStretch();
    return page;
}

QWidget *MainWindow::createAccountDetailsPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    addPageHeading(layout, QStringLiteral("Account details"),
                   QStringLiteral("Your personal account information."));

    auto *card = makeCard();
    auto *form = new QFormLayout(card);
    form->setContentsMargins(26, 24, 26, 24);
    form->setHorizontalSpacing(32);
    form->setVerticalSpacing(20);
    detailsName = makeLabel(QStringLiteral("—"), "transactionType");
    detailsNumber = makeLabel(QStringLiteral("—"), "transactionType");
    detailsBalance = makeLabel(QStringLiteral("Rs. 0.00"), "transactionType");
    form->addRow(makeLabel(QStringLiteral("Customer name"), "fieldLabel"), detailsName);
    form->addRow(makeLabel(QStringLiteral("Account number"), "fieldLabel"), detailsNumber);
    form->addRow(makeLabel(QStringLiteral("Available balance"), "fieldLabel"), detailsBalance);
    layout->addWidget(card);
    layout->addStretch();
    return page;
}

QWidget *MainWindow::createHistoryPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    addPageHeading(layout, QStringLiteral("Transaction history"),
                   QStringLiteral("Deposits and withdrawals recorded for this account."));

    auto *card = makeCard();
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(20, 17, 20, 16);
    auto *columns = new QHBoxLayout;
    columns->addWidget(makeLabel(QStringLiteral("ACTIVITY"), "muted"), 3);
    columns->addWidget(makeLabel(QStringLiteral("DATE"), "muted"), 2);
    columns->addWidget(makeLabel(QStringLiteral("AMOUNT"), "muted"), 1,
                       Qt::AlignRight);
    cardLayout->addLayout(columns);

    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    allTransactionsContainer = new QWidget;
    allTransactionsContainer->setLayout(new QVBoxLayout);
    allTransactionsContainer->layout()->setContentsMargins(0, 10, 0, 0);
    allTransactionsContainer->layout()->setSpacing(8);
    scroll->setWidget(allTransactionsContainer);
    cardLayout->addWidget(scroll);
    layout->addWidget(card, 1);
    return page;
}

QWidget *MainWindow::createPasswordPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    addPageHeading(layout, QStringLiteral("Password management"),
                   QStringLiteral("Update the password used to sign in."));

    auto *card = makeCard();
    card->setMaximumWidth(620);
    auto *form = new QVBoxLayout(card);
    form->setContentsMargins(26, 24, 26, 24);
    form->setSpacing(11);
    form->addWidget(makeLabel(QStringLiteral("Current password"), "fieldLabel"));
    currentPassword = makeInput(QStringLiteral("Enter current password"), true);
    form->addWidget(currentPassword);
    form->addWidget(makeLabel(QStringLiteral("New password"), "fieldLabel"));
    newPassword = makeInput(QStringLiteral("At least 8 characters"), true);
    form->addWidget(newPassword);
    form->addWidget(makeLabel(QStringLiteral("Confirm new password"), "fieldLabel"));
    newPasswordConfirm = makeInput(QStringLiteral("Repeat new password"), true);
    form->addWidget(newPasswordConfirm);
    auto *submit = makeButton(QStringLiteral("Update password"), "primary");
    form->addWidget(submit);
    layout->addWidget(card);
    layout->addStretch();
    connect(submit, &QPushButton::clicked, this, &MainWindow::changePassword);
    return page;
}

void MainWindow::createAccount()
{
    const QString name = signupName->text().trimmed();
    const QString number = signupNumber->text().trimmed();
    const QString password = signupPassword->text();
    if (name.isEmpty() || number.isEmpty()) {
        showMessage(QStringLiteral("Enter your name and account number."), true);
        return;
    }
    if (number.size() < 8
        || !QRegularExpression(QStringLiteral("^[0-9]+$")).match(number).hasMatch()) {
        showMessage(QStringLiteral("Invalid account number. Enter at least 8 digits only."), true);
        return;
    }
    if (password.size() < 8) {
        showMessage(QStringLiteral("Choose a password with at least 8 characters."), true);
        return;
    }
    if (password != signupPasswordConfirm->text()) {
        showMessage(QStringLiteral("The password confirmation does not match."), true);
        return;
    }
    double initialBalance = 0.0;
    if (!readAmount(signupBalance, &initialBalance)) {
        showMessage(QStringLiteral("Enter a valid initial balance."), true);
        return;
    }
    QLockFile accountLock(accountFilePath() + QStringLiteral(".lock"));
    if (!accountLock.tryLock(1000)) {
        showMessage(QStringLiteral("Account records are being updated. Please try again."),
                    true);
        return;
    }
    if (QFile::exists(accountFilePath()) && !loadAccount()) {
        showMessage(QStringLiteral("Saved accounts could not be checked. No account was created."),
                    true);
        return;
    }
    for (const Account &savedAccount : accounts) {
        if (savedAccount.number == number) {
            showMessage(QStringLiteral("That account number is already in use."), true);
            return;
        }
        if (savedAccount.name.compare(name, Qt::CaseInsensitive) == 0) {
            showMessage(QStringLiteral("An account with that customer name already exists."),
                        true);
            return;
        }
    }

    account.name = name;
    account.number = number;
    account.balance = initialBalance;
    account.passwordSalt = QUuid::createUuid().toString(QUuid::WithoutBraces);
    account.passwordHash = passwordDigest(account.passwordSalt, password);
    account.transactions.clear();
    accounts.append(account);

    if (!saveAccount()) {
        accounts.removeLast();
        account = Account{};
        showMessage(QStringLiteral("The account could not be saved. Check device storage and try again."),
                    true);
        return;
    }

    loginIdentity->setText(number);
    loginPassword->clear();
    signupName->clear();
    signupNumber->clear();
    signupPassword->clear();
    signupPasswordConfirm->clear();
    signupBalance->clear();
    fadeToPage(0);
    showMessage(QStringLiteral("Account created. Sign in with your account number and password."));
}

void MainWindow::login()
{
    if (accounts.isEmpty()) {
        showMessage(QStringLiteral("Create an account before signing in."), true);
        return;
    }

    const QString identity = loginIdentity->text().trimmed();
    const Account *matchedAccount = nullptr;
    for (const Account &savedAccount : accounts) {
        const bool identityMatches =
            identity == savedAccount.number
            || identity.compare(savedAccount.name, Qt::CaseInsensitive) == 0;
        if (identityMatches) {
            matchedAccount = &savedAccount;
            break;
        }
    }
    if (!matchedAccount) {
        showMessage(QStringLiteral("That account does not exist."), true);
        return;
    }
    if (passwordDigest(matchedAccount->passwordSalt, loginPassword->text())
        != matchedAccount->passwordHash) {
        showMessage(QStringLiteral("The password is incorrect."), true);
        return;
    }
    account = *matchedAccount;

    loginPassword->clear();
    if (!dashboardPage) {
        dashboardPage = createDashboardPage();
        pageStack->addWidget(dashboardPage);
    }
    refreshAccountViews();
    fadeToPage(pageStack->indexOf(dashboardPage));
    showFeature(Overview);
}

void MainWindow::logout()
{
    loginPassword->clear();
    fadeToPage(0);
}

void MainWindow::changePassword()
{
    if (passwordDigest(account.passwordSalt, currentPassword->text()) != account.passwordHash) {
        showMessage(QStringLiteral("Your current password is incorrect."), true);
        return;
    }
    if (newPassword->text().size() < 8) {
        showMessage(QStringLiteral("Choose a new password with at least 8 characters."), true);
        return;
    }
    if (newPassword->text() != newPasswordConfirm->text()) {
        showMessage(QStringLiteral("The new password confirmation does not match."), true);
        return;
    }

    const QString oldSalt = account.passwordSalt;
    const QString oldHash = account.passwordHash;
    account.passwordSalt = QUuid::createUuid().toString(QUuid::WithoutBraces);
    account.passwordHash = passwordDigest(account.passwordSalt, newPassword->text());
    if (!saveAccount()) {
        account.passwordSalt = oldSalt;
        account.passwordHash = oldHash;
        showMessage(QStringLiteral("The password could not be saved. Check device storage and try again."),
                    true);
        return;
    }

    currentPassword->clear();
    newPassword->clear();
    newPasswordConfirm->clear();
    showMessage(QStringLiteral("Your password has been updated."));
}

void MainWindow::performTransaction(bool isDeposit)
{
    double amount = 0.0;
    if (!readAmount(isDeposit ? depositAmount : withdrawAmount, &amount)) {
        showMessage(QStringLiteral("Enter a valid transaction amount."), true);
        return;
    }
    if (amount <= 0.0) {
        showMessage(QStringLiteral("Enter an amount greater than zero."), true);
        return;
    }
    if (!isDeposit && amount > account.balance) {
        showMessage(QStringLiteral("The withdrawal exceeds your available balance."), true);
        return;
    }

    const double previousBalance = account.balance;
    account.balance += isDeposit ? amount : -amount;
    account.transactions.append({isDeposit ? QStringLiteral("Deposit")
                                          : QStringLiteral("Withdrawal"),
                                amount, QDateTime::currentDateTime()});

    if (!saveAccount()) {
        account.balance = previousBalance;
        account.transactions.removeLast();
        showMessage(QStringLiteral("The transaction could not be saved. No changes were applied."),
                    true);
        return;
    }

    (isDeposit ? depositAmount : withdrawAmount)->clear();
    refreshAccountViews();
    showMessage(QStringLiteral("%1 of %2 completed.")
                    .arg(isDeposit ? QStringLiteral("Deposit") : QStringLiteral("Withdrawal"),
                         moneyText(amount)));
}

bool MainWindow::loadAccount()
{
    QFile file(accountFilePath());
    const bool loadLegacyFile = !file.exists();
    if (loadLegacyFile)
        file.setFileName(legacyAccountFilePath());
    if (!file.exists())
        return false;
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, QStringLiteral("Unable to load account"),
                             QStringLiteral("The saved account file could not be opened."));
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError
        || (!document.isObject() && !document.isArray())) {
        QMessageBox::warning(this, QStringLiteral("Unable to load account"),
                             QStringLiteral("The saved account data is invalid."));
        return false;
    }

    const auto parseAccount = [](const QJsonObject &object, Account *loaded) {
        loaded->name = object.value(QStringLiteral("name")).toString();
        loaded->number = object.value(QStringLiteral("number")).toString();
        loaded->balance = object.value(QStringLiteral("balance")).toDouble(-1.0);
        loaded->passwordSalt = object.value(QStringLiteral("passwordSalt")).toString();
        loaded->passwordHash = object.value(QStringLiteral("passwordHash")).toString();
        if (loaded->name.isEmpty() || loaded->number.isEmpty() || loaded->balance < 0.0
            || loaded->passwordSalt.isEmpty() || loaded->passwordHash.isEmpty())
            return false;

        const QJsonArray history = object.value(QStringLiteral("transactions")).toArray();
        for (const QJsonValue &value : history) {
            if (!value.isObject())
                return false;
            const QJsonObject transactionObject = value.toObject();
            const QString type = transactionObject.value(QStringLiteral("type")).toString();
            const double amount = transactionObject.value(QStringLiteral("amount")).toDouble(-1.0);
            const QDateTime timestamp =
                QDateTime::fromString(
                    transactionObject.value(QStringLiteral("timestamp")).toString(),
                    Qt::ISODateWithMs);
            if ((type != QStringLiteral("Deposit") && type != QStringLiteral("Withdrawal"))
                || amount <= 0.0 || !timestamp.isValid())
                return false;
            loaded->transactions.append({type, amount, timestamp});
        }
        return true;
    };

    const auto isEmptyPlaceholder = [](const QJsonObject &object) {
        return object.value(QStringLiteral("name")).isString()
            && object.value(QStringLiteral("name")).toString().isEmpty()
            && object.value(QStringLiteral("number")).isString()
            && object.value(QStringLiteral("number")).toString().isEmpty()
            && object.value(QStringLiteral("balance")).isDouble()
            && object.value(QStringLiteral("balance")).toDouble() == 0.0
            && object.value(QStringLiteral("passwordSalt")).isString()
            && object.value(QStringLiteral("passwordSalt")).toString().isEmpty()
            && object.value(QStringLiteral("passwordHash")).isString()
            && object.value(QStringLiteral("passwordHash")).toString().isEmpty()
            && object.value(QStringLiteral("transactions")).isArray()
            && object.value(QStringLiteral("transactions")).toArray().isEmpty();
    };

    QVector<Account> loadedAccounts;
    if (document.isObject()) {
        Account loaded;
        if (!parseAccount(document.object(), &loaded)) {
            QMessageBox::warning(this, QStringLiteral("Unable to load account"),
                                 QStringLiteral("The saved account data is invalid."));
            return false;
        }
        loadedAccounts.append(loaded);
    } else {
        for (const QJsonValue &value : document.array()) {
            if (!value.isObject()) {
                QMessageBox::warning(this, QStringLiteral("Unable to load account"),
                                     QStringLiteral("A saved account entry is invalid."));
                return false;
            }
            const QJsonObject object = value.toObject();
            if (isEmptyPlaceholder(object))
                continue;

            Account loaded;
            if (!parseAccount(object, &loaded)) {
                QMessageBox::warning(this, QStringLiteral("Unable to load account"),
                                     QStringLiteral("A saved account entry is invalid."));
                return false;
            }
            for (const Account &existing : loadedAccounts) {
                if (existing.number == loaded.number) {
                    QMessageBox::warning(this, QStringLiteral("Unable to load account"),
                                         QStringLiteral("Saved account numbers must be unique."));
                    return false;
                }
            }
            loadedAccounts.append(loaded);
        }
    }

    accounts = loadedAccounts;
    if (loadLegacyFile && !saveAccount()) {
        QMessageBox::warning(this, QStringLiteral("Account data migration"),
                             QStringLiteral("Your existing account data was loaded, but could not "
                                            "be copied beside the application. Check that the "
                                            "application folder is writable."));
    }
    return true;
}

bool MainWindow::saveAccount()
{
    const auto accountToJson = [](const Account &savedAccount) {
        QJsonObject object;
        object.insert(QStringLiteral("name"), savedAccount.name);
        object.insert(QStringLiteral("number"), savedAccount.number);
        object.insert(QStringLiteral("balance"), savedAccount.balance);
        object.insert(QStringLiteral("passwordSalt"), savedAccount.passwordSalt);
        object.insert(QStringLiteral("passwordHash"), savedAccount.passwordHash);

        QJsonArray history;
        for (const Transaction &transaction : savedAccount.transactions) {
            QJsonObject row;
            row.insert(QStringLiteral("type"), transaction.type);
            row.insert(QStringLiteral("amount"), transaction.amount);
            row.insert(QStringLiteral("timestamp"),
                       transaction.timestamp.toString(Qt::ISODateWithMs));
            history.append(row);
        }
        object.insert(QStringLiteral("transactions"), history);
        return object;
    };

    QVector<Account> updatedAccounts = accounts;
    bool updated = false;
    for (Account &savedAccount : updatedAccounts) {
        if (savedAccount.number == account.number) {
            savedAccount = account;
            updated = true;
            break;
        }
    }
    if (!updated)
        updatedAccounts.append(account);

    QJsonArray accountArray;
    for (const Account &savedAccount : updatedAccounts)
        accountArray.append(accountToJson(savedAccount));

    QSaveFile file(accountFilePath());
    if (!file.open(QIODevice::WriteOnly))
        return false;
    const QByteArray contents = QJsonDocument(accountArray).toJson(QJsonDocument::Indented);
    if (file.write(contents) != contents.size()) {
        file.cancelWriting();
        return false;
    }
    if (!file.commit())
        return false;
    accounts = updatedAccounts;
    return true;
}

QString MainWindow::passwordDigest(const QString &salt, const QString &password) const
{
    return QString::fromLatin1(
        QCryptographicHash::hash((salt + password).toUtf8(), QCryptographicHash::Sha256).toHex());
}

void MainWindow::showFeature(Feature feature)
{
    if (feature < Overview || feature >= FeatureCount)
        return;
    if (feature == Deposit)
        depositAmount->clear();
    else if (feature == Withdraw)
        withdrawAmount->clear();
    featureStack->setCurrentIndex(static_cast<int>(feature));
    for (int i = 0; i < navigationButtons.size(); ++i)
        navigationButtons.at(i)->setChecked(i == static_cast<int>(feature));
}

void MainWindow::refreshAccountViews()
{
    if (!overviewBalance)
        return;
    const QString amount = moneyText(account.balance);
    headerGreeting->setText(QStringLiteral("Welcome, %1").arg(account.name));
    headerAccountNumber->setText(QStringLiteral("Account %1").arg(account.number));
    overviewBalance->setText(amount);
    balanceValue->setText(amount);
    detailsName->setText(account.name);
    detailsNumber->setText(account.number);
    detailsBalance->setText(amount);
    refreshTransactionViews();
}

void MainWindow::refreshTransactionViews()
{
    const auto appendTransactionRow = [](QWidget *container, const Transaction &transaction,
                                         bool showDate) {
        auto *row = new QHBoxLayout;
        auto *type = makeLabel(
            transaction.type == QStringLiteral("Deposit")
                ? QStringLiteral("↗  Deposit")
                : QStringLiteral("↙  Withdrawal"),
            "transactionType");
        row->addWidget(type, showDate ? 3 : 1);
        if (showDate) {
            auto *date = makeLabel(
                transaction.timestamp.toLocalTime().toString(QStringLiteral("dd MMM yyyy, hh:mm")),
                "muted");
            row->addWidget(date, 2);
        }
        const bool positive = transaction.type == QStringLiteral("Deposit");
        auto *amount = makeLabel(
            QStringLiteral("%1%2")
                .arg(positive ? QStringLiteral("+") : QStringLiteral("−"),
                     moneyText(transaction.amount)),
            positive ? "positiveAmount" : "negativeAmount");
        row->addWidget(amount, 1, Qt::AlignRight);
        static_cast<QVBoxLayout *>(container->layout())->addLayout(row);
    };

    clearLayout(recentTransactionsContainer->layout());
    clearLayout(allTransactionsContainer->layout());
    if (account.transactions.isEmpty()) {
        recentTransactionsContainer->layout()->addWidget(
            makeLabel(QStringLiteral("No transactions yet. Your activity will appear here."),
                      "muted"));
        allTransactionsContainer->layout()->addWidget(
            makeLabel(QStringLiteral("No transactions have been recorded yet."), "muted"));
        return;
    }

    int displayed = 0;
    for (int i = account.transactions.size() - 1; i >= 0; --i) {
        if (displayed < 4)
            appendTransactionRow(recentTransactionsContainer,
                                 account.transactions.at(i), false);
        appendTransactionRow(allTransactionsContainer, account.transactions.at(i), true);
        ++displayed;
    }
}

void MainWindow::fadeToPage(int index)
{
    if (index == 1)
        signupBalance->clear();
    pageStack->setCurrentIndex(index);
}

void MainWindow::showMessage(const QString &message, bool isError)
{
    if (isError)
        QMessageBox::warning(this, QStringLiteral("Northstar"), message);
    else
        QMessageBox::information(this, QStringLiteral("Northstar"), message);
}
