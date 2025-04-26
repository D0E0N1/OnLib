#ifndef CLIENT_H
#define CLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QString>
#include <QDebug>

class Client : public QObject
{
    Q_OBJECT

private:
    static Client* p_instance;
    QTcpSocket* mTcpSocket;
    QString mRole; // Роль пользователя
    int mUserId;   // ID пользователя

    Client(QObject *parent = nullptr);

    bool mIsDisconnectingGracefully = false; // флаг

public:
    static Client& get_instance();
    static void destroy_instance();

    // Подключение к серверу
    void connectToServer(const QString& host, quint16 port);
    void sendRequest(const QString& request);
    void disconnectFromServer();

    // Пользовательские методы
    void extendRental(int book_id, int days);
    void viewBookStats();
    void viewUserInfo();
    void viewAllBooks();

    // метод для поиска книг
    void searchBooks(const QString& criteria, const QString& searchTerm);

    // Методы библиотекаря
    void assignBook(int user_id, int book_id);
    void unassignBook(int user_id, int book_id);
    void viewUserDebts(int user_id);
    void getUserInfo(const QString& login);
    void addBookToLibrary(const QString& title, const QString& author,
                          const QString& year, const QString& genre);
    void updateBookInfo(int book_id, const QString& title,
                        const QString& author, const QString& year,
                        const QString& genre);

    // Геттеры/сеттеры
    void setRole(const QString& role);
    QString getRole() const;
    void setUserId(int id);
    int getUserId() const;

    // Новые методы для аннотаций
    void getBookAnnotation(int book_id);
    void updateBookAnnotation(int book_id, const QString& annotation);

    // Новый метод для запроса списка пользователей
    void getAllUsers();

    void importBooksCsv(const QString& csvData);
    void exportBooksCsv();

    // Новые методы для управления пользователями
    void blockUser(int user_id);
    void unblockUser(int user_id);
    void resetUserPassword(int user_id, const QString& new_password);
    void updateUserEmail(int user_id, const QString& new_email);

    // Новый метод для запроса истории аренды
    void viewRentalHistory();

    // Новый метод для запроса статистики
    void getLibraryStats();

signals:
    void publicUserInfoReceived(int id, const QString& login, const QString& email);
    void booksListReceived(const QStringList& books);
    void responseReceived(const QString& response);
    void bookStatsReceived(const QStringList& stats); // [id, title, start_date, end_date]
    void userInfoReceived(const QString& login, const QString& email, const QString& password);
    void userDebtsReceived(const QStringList& debts); // [id, title, start_date, end_date]
    void operationStatusReceived(bool success, const QString& message);
    void overdueBooksNotification(const QStringList& overdueBooks);
    // Новый сигнал для полученной аннотации (book_id для идентификации, если запросы асинхронны)
    void bookAnnotationReceived(int book_id, const QString& title, const QString& annotation);
    // Новый сигнал для списка пользователей
    void allUsersListReceived(const QStringList& users); // Список строк "id,login"

    void exportCsvDataReceived(const QString& csvData);

    // Новый сигнал для истории аренды
    void rentalHistoryReceived(const QStringList& history);

    // Новый сигнал для статистики
    void libraryStatsReceived(int totalBooks, int availableBooks, int rentedBooks,
                              int totalClients, int activeRentals, int overdueRentals);

private slots:
    void onReadyRead();
    void onDisconnected();
};

#endif // CLIENT_H
