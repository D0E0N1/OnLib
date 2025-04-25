#ifndef DATABASE_H
#define DATABASE_H

#include <QCoreApplication>
#include <QSqlDatabase>
#include <QStringList>
#include <QSqlRecord>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QDebug>
#include <QtSql>

class database;
class database_destroyer
{
private:
    database *p_instance;
public:
    ~database_destroyer() { delete p_instance; }
    void initialize(database * p) { p_instance = p; }
};

class database {
private:
    QSqlDatabase my_database;
    static database * p_instance;
    static database_destroyer destroyer;

protected:
    database();
    database(const database&);
    database& operator = (database &);
    ~database();
    friend class database_destroyer;

public:
    // Common methods
    static database& get_instance();
    bool create_tables();
    int getUserID(const QString& login);
    QStringList getAllBooks();
    bool authenticateUser(const QString& login, const QString& password);
    bool registerUser(const QString& login, const QString& password,
                      const QString& email, const QString& role);
    QString getUserRole(const QString& login);
    // метод для получения списка пользователей (ID, Login)
    QStringList getAllUsers();

    // User methods
    QString getUserFullInfoById(int user_id);
    bool extendRental(int user_id, int book_id, int days);
    QString getUserBooksStats(int user_id);
    QString getUserFullInfo(const QString& login);

    // Librarian methods
    bool assignBookToUser(int user_id, int book_id);
    bool unassignBookFromUser(int user_id, int book_id);
    QStringList getUserDebts(int user_id);
    QString getUserPublicInfo(const QString& login);
    bool addBookToLibrary(const QString& title, const QString& author,
                          const QString& year, const QString& genre);
    bool updateBookInfo(int book_id, const QString& title,
                        const QString& author, const QString& year,
                        const QString& genre);

    QStringList getAllDebts();

    // Utility methods
    bool isBookAvailable(int book_id);
    bool userExists(int user_id);

    // метод для поиска книг
    QStringList searchBooks(const QString& criteria, const QString& searchTerm);

    // Новые методы для аннотаций
    QString getBookAnnotation(int book_id); // Получить аннотацию И название
    bool updateBookAnnotation(int book_id, const QString& annotation);
};

#endif // DATABASE_H
