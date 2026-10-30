#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QDateTime>
#include <QMainWindow>
#include <QString>
#include <QVector>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    struct Transaction {
        QString type;
        double amount = 0.0;
        QDateTime timestamp;
    };

    struct Account {
        QString name;
        QString number;
        double balance = 0.0;
        QString passwordSalt;
        QString passwordHash;
        QVector<Transaction> transactions;
    };

    enum Feature {
        Overview,
        Deposit,
        Withdraw,
        Balance,
        AccountDetails,
        History,
        PasswordManagement,
        FeatureCount
    };

    Ui::MainWindow *ui;
    QStackedWidget *pageStack = nullptr;
    QStackedWidget *featureStack = nullptr;
    QWidget *dashboardPage = nullptr;
    QVector<QPushButton *> navigationButtons;
    Account account;
    QVector<Account> accounts;

    QLineEdit *loginIdentity = nullptr;
    QLineEdit *loginPassword = nullptr;
    QLineEdit *signupName = nullptr;
    QLineEdit *signupNumber = nullptr;
    QLineEdit *signupPassword = nullptr;
    QLineEdit *signupPasswordConfirm = nullptr;
    QLineEdit *signupBalance = nullptr;
    QLineEdit *depositAmount = nullptr;
    QLineEdit *withdrawAmount = nullptr;
    QLineEdit *currentPassword = nullptr;
    QLineEdit *newPassword = nullptr;
    QLineEdit *newPasswordConfirm = nullptr;

    QLabel *headerGreeting = nullptr;
    QLabel *headerAccountNumber = nullptr;
    QLabel *overviewBalance = nullptr;
    QLabel *balanceValue = nullptr;
    QLabel *detailsName = nullptr;
    QLabel *detailsNumber = nullptr;
    QLabel *detailsBalance = nullptr;
    QWidget *recentTransactionsContainer = nullptr;
    QWidget *allTransactionsContainer = nullptr;

    QWidget *createLoginPage();
    QWidget *createSignupPage();
    QWidget *createDashboardPage();
    QWidget *createOverviewPage();
    QWidget *createDepositPage();
    QWidget *createWithdrawPage();
    QWidget *createBalancePage();
    QWidget *createAccountDetailsPage();
    QWidget *createHistoryPage();
    QWidget *createPasswordPage();

    bool loadAccount();
    bool saveAccount();
    QString passwordDigest(const QString &salt, const QString &password) const;
    void showFeature(Feature feature);
    void refreshAccountViews();
    void refreshTransactionViews();
    void performTransaction(bool isDeposit);
    void fadeToPage(int index);
    void showMessage(const QString &message, bool isError = false);

private slots:
    void createAccount();
    void login();
    void logout();
    void changePassword();
};

#endif // MAINWINDOW_H
