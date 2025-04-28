#include "database.h"
#include <QDateTime>
#include <QVariant> // Для работы со значениями из QSqlQuery
database* database::p_instance = nullptr;
database_destroyer database::destroyer;

database::~database() {
    my_database.close();
    qDebug() << "Database closed";
}

database& database::get_instance() {
    if (!p_instance) {
        p_instance = new database();
        destroyer.initialize(p_instance);
    }
    return *p_instance;
}

database::database() {
    my_database = QSqlDatabase::addDatabase("QSQLITE");
    my_database.setDatabaseName("mydatabase.db");

    if (!my_database.open()) {
        qDebug() << "Database error:" << my_database.lastError().text();
    } else {
        qDebug() << "Database opened successfully";
        create_tables();
    }
}

bool database::create_tables() {
    QSqlQuery query;
    bool success = true;

    // Users table
    success &= query.exec("CREATE TABLE IF NOT EXISTS users ("
                          "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                          "login TEXT UNIQUE NOT NULL, "
                          "password TEXT NOT NULL, "
                          "email TEXT NOT NULL, "
                          "role TEXT NOT NULL, "
                          "status TEXT DEFAULT 'active')"); // Новый столбец

    // Books table with genre
    success &= query.exec("CREATE TABLE IF NOT EXISTS books ("
                          "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                          "title TEXT NOT NULL, "
                          "author TEXT NOT NULL, "
                          "year TEXT NOT NULL, "
                          "genre TEXT NOT NULL, "
                          "is_available BOOLEAN DEFAULT 1, "
                          "annotation TEXT DEFAULT '')");    // Новый столбец

    // Rentals table
    success &= query.exec("CREATE TABLE IF NOT EXISTS rentals ("
                          "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                          "user_id INTEGER NOT NULL, "
                          "book_id INTEGER NOT NULL, "
                          "start_date TEXT NOT NULL, "
                          "end_date TEXT NOT NULL, "
                          "FOREIGN KEY(user_id) REFERENCES users(id), "
                          "FOREIGN KEY(book_id) REFERENCES books(id))");


    // Новая таблица для ИСТОРИИ аренды
    success &= query.exec("CREATE TABLE IF NOT EXISTS rental_history ("
                          "id INTEGER PRIMARY KEY AUTOINCREMENT, "        // ID записи истории
                          "user_id INTEGER NOT NULL, "
                          "book_id INTEGER NOT NULL, "
                          "start_date TEXT NOT NULL, "                  // Дата начала аренды
                          "end_date TEXT NOT NULL, "                    // ПЛАНОВАЯ дата возврата
                          "return_date TEXT NOT NULL, "                 // ФАКТИЧЕСКАЯ дата возврата
                          "FOREIGN KEY(user_id) REFERENCES users(id), "
                          "FOREIGN KEY(book_id) REFERENCES books(id))");

    if (!success) {
        qDebug() << "Error creating/updating tables:" << query.lastError();
            // Попытка добавить столбец status, если таблица уже существует
        if (!query.exec("ALTER TABLE users ADD COLUMN status TEXT DEFAULT 'active'")) {
            qDebug() << "Failed to add status column to users table (might already exist):" << query.lastError();
        } else {
            qDebug() << "Successfully added 'status' column to 'users' table.";
        }
        success = true; // Продолжаем работу
    }
    return success;
}

// Common methods
bool database::authenticateUser(const QString& login, const QString& password) {
    QSqlQuery query;
        // Выбираем пароль и статус
    query.prepare("SELECT password, status FROM users WHERE login = :login");
    query.bindValue(":login", login);

    if (!query.exec()) {
        qDebug() << "Auth error:" << query.lastError().text();
        return false;
    }

    if (query.next()) {
        QString dbPassword = query.value(0).toString();
        QString dbStatus = query.value(1).toString();
            // Проверяем и пароль, и статус

        qDebug() << "Authenticating user:" << login;
        qDebug() << "  DB Password (Plain) :" << dbPassword;
        qDebug() << "  Input Password (Plain):" << password;
        qDebug() << "  DB Status           :" << dbStatus;
        qDebug() << "  Password Match      :" << (dbPassword == password);
        qDebug() << "  Status Match        :" << (dbStatus == "active");

        return (dbPassword == password && dbStatus == "active");
    }
    return false; // Пользователь не найден
}

int database::getUserID(const QString& login) {
    QSqlQuery query;
    query.prepare("SELECT id FROM users WHERE login = :login");
    query.bindValue(":login", login);

    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    return -1;
}

QStringList database::getAllBooks() {
    QSqlQuery query("SELECT id, title, author, year, genre, is_available, annotation FROM books");
    QStringList books;
    while (query.next()) {
        books << QString("%1,%2,%3,%4,%5,%6,%7")
                     .arg(query.value(0).toString()) // id
                     .arg(query.value(1).toString()) // title
                     .arg(query.value(2).toString()) // author
                     .arg(query.value(3).toString()) // year
                     .arg(query.value(4).toString()) // genre
                     .arg(query.value(5).toString()) // is_available
                     .arg(query.value(6).toString()); // annotation
    }
    return books;
}

bool database::registerUser(const QString& login, const QString& password,
                            const QString& email, const QString& role) {
    QSqlQuery checkQuery;
    checkQuery.prepare("SELECT login FROM users WHERE login = :login");
    checkQuery.bindValue(":login", login);

    if (!checkQuery.exec() || checkQuery.next()) {
        qDebug() << "Registration error: User already exists or query failed";
        return false;
    }

    QSqlQuery insertQuery;
    insertQuery.prepare("INSERT INTO users (login, password, email, role) "
                        "VALUES (:login, :password, :email, :role)");
    insertQuery.bindValue(":login", login);
    insertQuery.bindValue(":password", password);
    insertQuery.bindValue(":email", email);
    insertQuery.bindValue(":role", role);

    if (!insertQuery.exec()) {
        qDebug() << "Insert error:" << insertQuery.lastError().text();
        return false;
    }
    return true;
}

QString database::getUserRole(const QString& login) {
    QSqlQuery query;
    query.prepare("SELECT role FROM users WHERE login = :login");
    query.bindValue(":login", login);

    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return "";
}

// User methods
bool database::extendRental(int user_id, int book_id, int days) {
    // 1. Проверка существования пользователя
    if (!userExists(user_id)) {
        qDebug() << "Extend error: User" << user_id << "does not exist";
        return false;
    }

    // 2. Проверка что книга НЕ доступна (должна быть выдана)
    if (isBookAvailable(book_id)) {
        qDebug() << "Extend error: Book" << book_id << "is not assigned";
        return false;
    }

    // 3. Проверка существования активной аренды
    QSqlQuery checkRental;
    checkRental.prepare("SELECT id FROM rentals WHERE user_id = :user_id AND book_id = :book_id");
    checkRental.bindValue(":user_id", user_id);
    checkRental.bindValue(":book_id", book_id);

    if (!checkRental.exec() || !checkRental.next()) {
        qDebug() << "Extend error: No active rental for user" << user_id << "book" << book_id;
        return false;
    }

    // 4. Обновление даты возврата
    QSqlQuery query;
    QString dateModifier = QString("+%1 days").arg(days); // Формируем модификатор
    query.prepare("UPDATE rentals SET end_date = DATE(end_date, :modifier) "
                  "WHERE user_id = :user_id AND book_id = :book_id");
    query.bindValue(":modifier", dateModifier); // Явная передача модификатора
    query.bindValue(":user_id", user_id);
    query.bindValue(":book_id", book_id);

    if (!query.exec()) {
        qDebug() << "Extend error:" << query.lastError().text();
        return false;
    }

    // 5. Проверка что запись действительно обновилась
    return (query.numRowsAffected() > 0);
}

QString database::getUserBooksStats(int user_id) {
    if (!userExists(user_id)) return "";

    QSqlQuery query;
    query.prepare("SELECT b.id, b.title, r.start_date, r.end_date "
                  "FROM rentals r JOIN books b ON r.book_id = b.id "
                  "WHERE r.user_id = :user_id");
    query.bindValue(":user_id", user_id);

    if (!query.exec()) {
        qDebug() << "Error getting user books stats:" << query.lastError().text();
        return "";
    }

    QStringList stats;
    while (query.next()) {
        stats << QString("%1,%2,%3,%4") // Формат: id,title,start_date,end_date
                     .arg(query.value(0).toString())
                     .arg(query.value(1).toString())
                     .arg(query.value(2).toString())
                     .arg(query.value(3).toString());
    }
    return stats.join("|");
}

QString database::getUserFullInfo(const QString& login) {
    QSqlQuery query;
    query.prepare("SELECT login, password, email, role FROM users WHERE login = :login");
    query.bindValue(":login", login);

    if (query.exec() && query.next()) {
        return QString("%1,%2,%3,%4")
        .arg(query.value(0).toString())
            .arg(query.value(1).toString())
            .arg(query.value(2).toString())
            .arg(query.value(3).toString());
    }
    return "";
}

// Librarian methods
bool database::assignBookToUser(int user_id, int book_id) {
    // Начало транзакции
    QSqlDatabase::database().transaction();

    try {
        // Проверка существования пользователя
        if (!userExists(user_id)) {
            QSqlDatabase::database().rollback();
            qDebug() << "User does not exist";
            return false;
        }

        // Проверка доступности книги
        if (!isBookAvailable(book_id)) {
            QSqlDatabase::database().rollback();
            qDebug() << "Book is not available";
            return false;
        }

        // Вставка записи о выдаче
        QSqlQuery insertQuery;
        insertQuery.prepare(
            "INSERT INTO rentals (user_id, book_id, start_date, end_date) "
            "VALUES (:user_id, :book_id, DATE('now'), DATE('now', '+14 days'))"
            );
        insertQuery.bindValue(":user_id", user_id);
        insertQuery.bindValue(":book_id", book_id);

        if (!insertQuery.exec()) {
            qDebug() << "Insert error:" << insertQuery.lastError().text();
            QSqlDatabase::database().rollback();
            return false;
        }

        // Обновление статуса книги
        QSqlQuery updateQuery;
        updateQuery.prepare(
            "UPDATE books SET is_available = 0 "
            "WHERE id = :book_id"
            );
        updateQuery.bindValue(":book_id", book_id);

        if (!updateQuery.exec()) {
            qDebug() << "Update error:" << updateQuery.lastError().text();
            QSqlDatabase::database().rollback();
            return false;
        }

        // Фиксация транзакции
        QSqlDatabase::database().commit();
        return true;

    } catch (const std::exception& e) {
        QSqlDatabase::database().rollback();
        qDebug() << "Exception:" << e.what();
        return false;
    }
}

bool database::unassignBookFromUser(int user_id, int book_id) {
    if (!userExists(user_id)) return false;

    QSqlDatabase::database().transaction(); // ★ Начинаем транзакцию ★

    // 1. Получаем данные УДАЛЯЕМОЙ аренды
    QSqlQuery selectQuery;
    selectQuery.prepare("SELECT start_date, end_date FROM rentals "
                        "WHERE user_id = :user_id AND book_id = :book_id");
    selectQuery.bindValue(":user_id", user_id);
    selectQuery.bindValue(":book_id", book_id);

    QString startDate, endDate;
    if (selectQuery.exec() && selectQuery.next()) {
        startDate = selectQuery.value(0).toString();
        endDate = selectQuery.value(1).toString();
    } else {
        qWarning() << "Rental record not found for user" << user_id << "book" << book_id << "Cannot move to history.";
        QSqlDatabase::database().rollback(); // Откатываем транзакцию
        return false; // Аренда не найдена, нечего перемещать/удалять
    }

    // 2. Вставляем запись в ИСТОРИЮ с текущей датой возврата
    QSqlQuery insertHistoryQuery;
    insertHistoryQuery.prepare("INSERT INTO rental_history (user_id, book_id, start_date, end_date, return_date) "
                               "VALUES (:user_id, :book_id, :start_date, :end_date, DATE('now'))");
    insertHistoryQuery.bindValue(":user_id", user_id);
    insertHistoryQuery.bindValue(":book_id", book_id);
    insertHistoryQuery.bindValue(":start_date", startDate);
    insertHistoryQuery.bindValue(":end_date", endDate); // Сохраняем плановую дату

    if (!insertHistoryQuery.exec()) {
        qDebug() << "Error moving rental to history:" << insertHistoryQuery.lastError().text();
        QSqlDatabase::database().rollback(); // Откатываем транзакцию
        return false;
    }
    qDebug() << "Moved rental to history for user" << user_id << "book" << book_id;

    // 3. Удаляем запись из АКТИВНЫХ аренд
    QSqlQuery deleteQuery;
    deleteQuery.prepare("DELETE FROM rentals WHERE user_id = :user_id AND book_id = :book_id");
    deleteQuery.bindValue(":user_id", user_id);
    deleteQuery.bindValue(":book_id", book_id);

    if (!deleteQuery.exec()) {
        qDebug() << "Error deleting from active rentals after moving to history:" << deleteQuery.lastError().text();
        QSqlDatabase::database().rollback(); // Откатываем транзакцию
        return false;
    }

    // 4. Делаем книгу снова доступной
    QSqlQuery updateBookQuery;
    updateBookQuery.prepare("UPDATE books SET is_available = 1 WHERE id = :book_id");
    updateBookQuery.bindValue(":book_id", book_id);

    if (!updateBookQuery.exec()) {
        qDebug() << "Error updating book status after unassign:" << updateBookQuery.lastError().text();
        QSqlDatabase::database().rollback(); // Откатываем транзакцию
        return false;
    }

    // ★ Фиксируем транзакцию, если все успешно ★
    if (!QSqlDatabase::database().commit()) {
        qDebug() << "Transaction commit failed during unassign!";
        QSqlDatabase::database().rollback(); // Попытка отката при ошибке commit
        return false;
    }

    return true; // Все шаги прошли успешно
}

QStringList database::getUserDebts(int user_id) {
    if (!userExists(user_id)) return QStringList();

    QSqlQuery query;
    QString sql =
        "SELECT r.id AS debt_id, b.title, u.login, "
        "r.start_date, r.end_date, "
        "CASE WHEN DATE(r.end_date) < DATE('now') THEN '1' ELSE '0' END AS is_overdue "
        "FROM rentals r "
        "JOIN books b ON r.book_id = b.id "
        "JOIN users u ON r.user_id = u.id "
        "WHERE r.user_id = :user_id";

    query.prepare(sql);
    query.bindValue(":user_id", user_id);

    if (!query.exec()) {
        qDebug() << "SQL Error:" << query.lastError().text();
        return QStringList();
    }

    QStringList debts;
    while (query.next()) {
        debts << QString("%1,%2,%3,%4,%5,%6")
        .arg(query.value("debt_id").toString())
            .arg(query.value("title").toString())
            .arg(query.value("login").toString())
            .arg(query.value("start_date").toString())
            .arg(query.value("end_date").toString())
            .arg(query.value("is_overdue").toString());
    }
    return debts;
}

QString database::getUserPublicInfo(const QString& login) {
    QSqlQuery query;
    query.prepare("SELECT id, login, email FROM users WHERE login = :login"); // Добавлен id
    query.bindValue(":login", login);

    if (query.exec() && query.next()) {
        return QString("%1,%2,%3")  // Формат: id,login,email
            .arg(query.value(0).toString()) // id
            .arg(query.value(1).toString()) // login
            .arg(query.value(2).toString()); // email
    }
    return "";
}
bool database::addBookToLibrary(const QString& title, const QString& author,
                                const QString& year, const QString& genre) {
    QSqlQuery query;
    query.prepare("INSERT INTO books (title, author, year, genre) "
                  "VALUES (:title, :author, :year, :genre)");
    query.bindValue(":title", title);
    query.bindValue(":author", author);
    query.bindValue(":year", year);
    query.bindValue(":genre", genre);

    if (!query.exec()) {
        qDebug() << "Error adding book to library:" << query.lastError().text();
        return false;
    }
    qDebug() << "Book added successfully!"; // Добавить подтверждение
    return true;
}

bool database::updateBookInfo(int book_id, const QString& title,
                              const QString& author, const QString& year,
                              const QString& genre) {
    QSqlQuery query;
    query.prepare("UPDATE books SET title = :title, author = :author, "
                  "year = :year, genre = :genre WHERE id = :book_id");
    query.bindValue(":title", title);
    query.bindValue(":author", author);
    query.bindValue(":year", year);
    query.bindValue(":genre", genre);
    query.bindValue(":book_id", book_id);

    if (!query.exec()) {
        qDebug() << "Error updating book info:" << query.lastError().text();
        return false;
    }
    return true;
}

// Utility methods
bool database::isBookAvailable(int book_id) {
    QSqlQuery query;
    query.prepare("SELECT is_available FROM books WHERE id = :book_id");
    query.bindValue(":book_id", book_id);

    if (!query.exec()) {
        qDebug() << "Book availability check error:" << query.lastError().text();
        return false;
    }

    return query.next() && query.value(0).toBool(); // True = доступна, False = выдана
}


bool database::userExists(int user_id) {
    QSqlQuery query;
    query.prepare("SELECT id FROM users WHERE id = :user_id");
    query.bindValue(":user_id", user_id);
    return (query.exec() && query.next());
}

QStringList database::getAllDebts() {
    QSqlQuery query;
    QString sql =
        "SELECT r.id, u.login, b.title, r.start_date, r.end_date, "
        "CASE WHEN DATE(r.end_date) < DATE('now') THEN '1' ELSE '0' END "
        "FROM rentals r "
        "JOIN users u ON r.user_id = u.id "
        "JOIN books b ON r.book_id = b.id";

    if (!query.exec(sql)) {
        qDebug() << "SQL Error:" << query.lastError().text();
        return QStringList();
    }

    QStringList debts;
    while (query.next()) {
        debts << QString("%1,%2,%3,%4,%5,%6")
        .arg(query.value(0).toString())  // ID записи
            .arg(query.value(1).toString())  // Логин пользователя
            .arg(query.value(2).toString())  // Название книги
            .arg(query.value(3).toString())  // Дата начала
            .arg(query.value(4).toString())  // Дата окончания
            .arg(query.value(5).toString()); // Просрочка
    }
    return debts;
}

QString database::getUserFullInfoById(int user_id) {
    QSqlQuery query;
    query.prepare("SELECT login, email, password FROM users WHERE id = :user_id");
    query.bindValue(":user_id", user_id);

    if (query.exec() && query.next()) {
        return QString("%1,%2,%3")  // Теперь: login, email, password
            .arg(query.value(0).toString())
            .arg(query.value(1).toString())
            .arg(query.value(2).toString());
    }
    return "";
}

// Реализация нового метода поиска книг
QStringList database::searchBooks(const QString& criteria, const QString& searchTerm) {
    QStringList books;
    QSqlQuery query;
    QString queryString = "SELECT id, title, author, year, genre, is_available FROM books WHERE ";

    // Формируем WHERE clause в зависимости от критерия
    // Используем LOWER() для регистронезависимого поиска и LIKE для частичного совпадения
    if (criteria == "title") {
        queryString += "LOWER(title) LIKE LOWER(:term)";
    } else if (criteria == "author") {
        queryString += "LOWER(author) LIKE LOWER(:term)";
    } else if (criteria == "genre") {
        queryString += "LOWER(genre) LIKE LOWER(:term)";
    } else {
        qWarning() << "Invalid search criteria:" << criteria;
        return books; // Возвращаем пустой список при неверном критерии
    }

    query.prepare(queryString);
    // Добавляем wildcards (%) для поиска подстроки и используем bindValue для безопасности
    query.bindValue(":term", "%" + searchTerm + "%");

    if (!query.exec()) {
        qDebug() << "Search books error:" << query.lastError().text();
        return books; // Возвращаем пустой список в случае ошибки запроса
    }

    // Формируем список так же, как в getAllBooks
    while (query.next()) {
        books << QString("%1,%2,%3,%4,%5,%6")
        .arg(query.value(0).toString()) // id
            .arg(query.value(1).toString()) // title
            .arg(query.value(2).toString()) // author
            .arg(query.value(3).toString()) // year
            .arg(query.value(4).toString()) // genre
            .arg(query.value(5).toString()); // is_available
    }
    return books;
}

// Получение аннотации И НАЗВАНИЯ книги по ID
QString database::getBookAnnotation(int book_id) {
    QSqlQuery query;
    // Выбираем и аннотацию, и название
    query.prepare("SELECT title, annotation FROM books WHERE id = :book_id");
    query.bindValue(":book_id", book_id);

    if (!query.exec()) {
        qDebug() << "Get book annotation error:" << query.lastError().text();
        return ""; // Возвращаем пустую строку в случае ошибки
    }

    if (query.next()) {
        QString title = query.value(0).toString();
        QString annotation = query.value(1).toString();
        // Возвращаем строку в формате "title,annotation"
        // Используем разделитель, который маловероятен в тексте, например, `:::`
        return QString("%1:::%2").arg(title, annotation);
    }
    return ""; // Книга не найдена
}

// Обновление аннотации книги по ID
bool database::updateBookAnnotation(int book_id, const QString& annotation) {
    QSqlQuery query;
    query.prepare("UPDATE books SET annotation = :annotation WHERE id = :book_id");
    query.bindValue(":annotation", annotation);
    query.bindValue(":book_id", book_id);

    if (!query.exec()) {
        qDebug() << "Update book annotation error:" << query.lastError().text();
        return false;
    }
    // Проверяем, была ли запись реально обновлена
    return query.numRowsAffected() > 0;
}

// Реализация метода получения списка всех пользователей
QStringList database::getAllUsers() {
    QStringList users;
    // ИЗМЕНЕН ЗАПРОС: Добавлено условие WHERE role = 'client'
    QSqlQuery query("SELECT id, login, status FROM users WHERE role = 'client' ORDER BY login ASC");


    if (!query.exec()) {
        qDebug() << "Error getting client users:" << query.lastError().text();
        return users; // Возвращаем пустой список в случае ошибки
    }

    while (query.next()) {
        // ★ Добавляем статус в формируемую строку ★
        users << QString("%1,%2,%3")
                     .arg(query.value(0).toString()) // id
                     .arg(query.value(1).toString()) // login
                     .arg(query.value(2).toString()); // status
    }
    return users;
}

// Обновление статуса пользователя
bool database::setUserStatus(int user_id, const QString& status) {
    if (status != "active" && status != "blocked") {
        qWarning() << "Invalid status provided to setUserStatus:" << status;
        return false;
    }
    QSqlQuery query;
    query.prepare("UPDATE users SET status = :status WHERE id = :user_id");
    query.bindValue(":status", status);
    query.bindValue(":user_id", user_id);
    if (!query.exec()) {
        qDebug() << "Error updating user status:" << query.lastError().text();
        return false;
    }
    return query.numRowsAffected() > 0; // Возвращаем true, если строка была обновлена
}

// Обновление (сброс) пароля пользователя (принимает хеш)
bool database::resetUserPassword(int user_id, const QString& new_password) { // ★ Сигнатура изменена ★
    QSqlQuery query;
    query.prepare("UPDATE users SET password = :password WHERE id = :user_id");
    query.bindValue(":password", new_password); // ★ Сохраняем пароль как есть ★
    query.bindValue(":user_id", user_id);
    qDebug() << "Executing password reset query for user ID:" << user_id;
    if (!query.exec()) {
        qDebug() << "!!! SQL Error resetting password:" << query.lastError().text();
        return false;
    }
    qDebug() << "Password reset query executed. Rows affected:" << query.numRowsAffected();
    return query.numRowsAffected() > 0;
}
// Обновление email пользователя с проверкой уникальности
bool database::updateUserEmail(int user_id, const QString& new_email) {
    // 1. Проверка, не занят ли новый email другим пользователем
    QSqlQuery checkQuery;
    checkQuery.prepare("SELECT id FROM users WHERE email = :email AND id != :user_id");
    checkQuery.bindValue(":email", new_email);
    checkQuery.bindValue(":user_id", user_id);
    if (!checkQuery.exec()) {
        qDebug() << "Email uniqueness check failed:" << checkQuery.lastError().text();
        return false; // Ошибка проверки
    }
    if (checkQuery.next()) {
        qDebug() << "Email already taken by another user:" << new_email;
        return false; // Email занят
    }

    // 2. Если email свободен, обновляем
    QSqlQuery updateQuery;
    updateQuery.prepare("UPDATE users SET email = :email WHERE id = :user_id");
    updateQuery.bindValue(":email", new_email);
    updateQuery.bindValue(":user_id", user_id);
    if (!updateQuery.exec()) {
        qDebug() << "Error updating user email:" << updateQuery.lastError().text();
        return false;
    }
    return updateQuery.numRowsAffected() > 0;
}

// Получение текущего статуса пользователя
QString database::getUserStatus(int user_id) {
    QSqlQuery query;
    query.prepare("SELECT status FROM users WHERE id = :user_id");
    query.bindValue(":user_id", user_id);
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return ""; // Или какое-то значение по умолчанию/ошибки
}


// Реализация метода получения ИСТОРИИ аренды пользователя
QStringList database::getUserRentalHistory(int user_id) {
    QStringList historyList;
    QSqlQuery query;
    // Выбираем ID истории, Название книги, Даты начала, планового конца и фактического возврата
    query.prepare("SELECT rh.id, b.title, rh.start_date, rh.end_date, rh.return_date "
                  "FROM rental_history rh "
                  "JOIN books b ON rh.book_id = b.id "
                  "WHERE rh.user_id = :user_id "
                  "ORDER BY rh.return_date DESC"); // Сортируем по дате возврата (последние сверху)
    query.bindValue(":user_id", user_id);

    if (!query.exec()) {
        qDebug() << "Error getting user rental history:" << query.lastError().text();
        return historyList;
    }

    while (query.next()) {
        historyList << QString("%1,%2,%3,%4,%5")
        .arg(query.value(0).toString()) // history id
            .arg(query.value(1).toString()) // book title
            .arg(query.value(2).toString()) // start date
            .arg(query.value(3).toString()) // scheduled end date
            .arg(query.value(4).toString()); // actual return date
    }
    return historyList;
}

// Получение общего количества книг
int database::getTotalBookCount() {
    QSqlQuery query("SELECT COUNT(*) FROM books");
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    qWarning() << "Failed to get total book count:" << query.lastError();
    return -1; // Ошибка
}

// Получение количества доступных книг
int database::getAvailableBookCount() {
    QSqlQuery query("SELECT COUNT(*) FROM books WHERE is_available = 1");
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    qWarning() << "Failed to get available book count:" << query.lastError();
    return -1; // Ошибка
}

// Получение общего количества клиентов
int database::getTotalClientCount() {
    QSqlQuery query("SELECT COUNT(*) FROM users WHERE role = 'client'");
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    qWarning() << "Failed to get total client count:" << query.lastError();
    return -1; // Ошибка
}

// Получение количества активных аренд
int database::getActiveRentalCount() {
    QSqlQuery query("SELECT COUNT(*) FROM rentals");
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    qWarning() << "Failed to get active rental count:" << query.lastError();
    return -1; // Ошибка
}

// Получение количества просроченных аренд
int database::getOverdueRentalCount() {
    QSqlQuery query("SELECT COUNT(*) FROM rentals WHERE DATE(end_date) < DATE('now')");
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    qWarning() << "Failed to get overdue rental count:" << query.lastError();
    return -1; // Ошибка
}

// Получение списка уникальных жанров из таблицы книг (для фильтра отчетов)
QStringList database::getGenres() {
    QStringList genres;
    // Выбираем уникальные непустые значения из колонки 'genre' и сортируем их
    QSqlQuery query("SELECT DISTINCT genre FROM books WHERE genre IS NOT NULL AND genre != '' ORDER BY genre ASC");
    if (!query.exec()) {
        qWarning() << "Failed to retrieve genres from database:" << query.lastError();
        return genres; // Возвращаем пустой список в случае ошибки
    }
    // Добавляем каждый полученный жанр в список
    while (query.next()) {
        genres << query.value(0).toString();
    }
    return genres;
}

// --- Генерация данных для детальных отчетов ---
// Основной метод для получения данных для различных типов отчетов.
// Принимает:
//   - reportType: Строка, идентифицирующая запрашиваемый отчет.
//   - startDateStr: Начальная дата периода в формате 'yyyy-MM-dd'.
//   - endDateStr: Конечная дата периода в формате 'yyyy-MM-dd'.
//   - optionalFilter: Дополнительный фильтр (например, жанр), может быть пустым.
// Возвращает строку с данными в одном из форматов:
//   - Таблица: "Заг1,Заг2|Данные11,Данные12|Данные21,Данные22..."
//   - График:  "chart:ТипОтчета&Метка1,Метка2&Значение1,Значение2..."
//   - Ошибка:  "error:Сообщение об ошибке"
//   - Нет данных: "" (пустая строка, либо "Заголовки|" для пустой таблицы)
QString database::getReportData(const QString& reportType, const QString& startDateStr, const QString& endDateStr, const QString& optionalFilter) {
    QSqlQuery query(my_database); // Используем соединение текущего объекта database
    QString sql;                   // Строка для SQL запроса
    QStringList headers;           // Заголовки для табличного отчета
    QStringList dataRows;          // Строки данных для табличного отчета (каждая строка - "val1,val2...")
    QStringList chartLabels;       // Метки для осей/срезов графика
    QList<qreal> chartValues;      // Числовые значения для графика
    bool isChartData = false;      // Флаг: true - результат для графика, false - для таблицы

    // Подготовка общих фрагментов SQL для фильтрации по дате.
    // Важно: Убраны лишние пробелы в конце для корректной конкатенации с AND.
    QString dateConditionRentals = QString("WHERE DATE(r.start_date) BETWEEN DATE('%1') AND DATE('%2')").arg(startDateStr, endDateStr);
    QString dateConditionHistory = QString("WHERE DATE(rh.start_date) BETWEEN DATE('%1') AND DATE('%2')").arg(startDateStr, endDateStr); // По дате НАЧАЛА аренды
    QString dateConditionReturns = QString("WHERE DATE(rh.return_date) BETWEEN DATE('%1') AND DATE('%2')").arg(startDateStr, endDateStr); // По дате ВОЗВРАТА

    // Подготовка фрагмента SQL для опционального фильтра (например, по жанру)
    // Важно: Используем prepare/bindValue для безопасности от SQL-инъекций!
    QString filterClause;    // Часть SQL-запроса с AND для фильтра
    bool useFilterBinding = !optionalFilter.isEmpty(); // Флаг, нужно ли биндить значение фильтра

    qDebug() << "[Database] getReportData: Initial useFilterBinding=" << useFilterBinding << "for filter:" << optionalFilter;

    if (useFilterBinding) {
        // Явно проверяем типы отчетов, к которым применим СТАНДАРТНЫЙ фильтр по books.genre (алиас 'b')
        if (reportType == "Популярность книг (Топ-10)" ||
            reportType == "Текущие просрочки" ||
            reportType == "Активность (Выдачи/Возвраты)") // <<< ДОБАВЛЯЕМ "Активность" СЮДА
        {
            filterClause = " AND b.genre = :filterValue ";
            // useFilterBinding остается true
            qDebug() << "[Database] Applying standard genre filter (b.genre) for report:" << reportType;
        }
        // Явно проверяем типы отчетов с ДРУГОЙ логикой фильтра
        else if (reportType == "Активные читатели (Топ-10)")
        {
            filterClause = " AND rh.book_id IN (SELECT id FROM books WHERE genre = :filterValue) ";
            // useFilterBinding остается true
            qDebug() << "[Database] Applying specific genre filter (reader activity) for report:" << reportType;
        }
        // Явно проверяем типы отчетов, где фильтр НЕ НУЖЕН (они сами группируют по жанрам)
        else if (reportType == "Популярность жанров" || reportType == "Состав фонда по жанрам")
        {
            useFilterBinding = false; // Фильтр здесь не применяется
            qDebug() << "[Database] Genre filter is not applicable for report:" << reportType;
        }
        // Обработка всех остальных (неизвестных или новых) типов отчетов
        else
        {
            qWarning() << "Genre filter application is undefined for report type:" << reportType << "- Disabling filter.";
            useFilterBinding = false; // Безопаснее отключить фильтр
        }
    }

    // =============================================
    // Основной блок: Определение и выполнение SQL-запроса
    // =============================================
    try {
        if (reportType == "Активность (Выдачи/Возвраты)") {
            isChartData = true;
            chartLabels << "Выдачи" << "Возвраты";

            // --- Запрос для ВЫДАЧ ---
            sql = "SELECT COUNT(r.id) FROM rentals r "; // Базовый запрос
            QString whereClauseRentals = dateConditionRentals; // Начинается с WHERE date...

            if (useFilterBinding) {
                sql += "JOIN books b ON r.book_id = b.id "; // Добавляем JOIN ТОЛЬКО при фильтре
                whereClauseRentals += filterClause;        // Добавляем условие жанра к WHERE (filterClause начинается с AND)
            }
            sql += whereClauseRentals; // Добавляем собранное условие WHERE

            query.prepare(sql);
            if (useFilterBinding) {
                query.bindValue(":filterValue", optionalFilter);
            }
            if (!query.exec()) {
                qWarning() << "SQL Error (Rentals Count):" << query.lastError().text() << "Query:" << query.lastQuery();
                throw query.lastError(); // Передаем ошибку дальше
            }
            chartValues << (query.next() ? query.value(0).toReal() : 0.0);

            // --- Запрос для ВОЗВРАТОВ ---
            sql = "SELECT COUNT(rh.id) FROM rental_history rh "; // Базовый запрос
            QString whereClauseHistory = dateConditionReturns; // Начинается с WHERE date...

            if (useFilterBinding) {
                sql += "JOIN books b ON rh.book_id = b.id "; // Добавляем JOIN ТОЛЬКО при фильтре
                whereClauseHistory += filterClause;        // Добавляем условие жанра к WHERE (filterClause начинается с AND)
            }
            sql += whereClauseHistory; // Добавляем собранное условие WHERE

            query.prepare(sql);
            if (useFilterBinding) {
                query.bindValue(":filterValue", optionalFilter);
            }
            if (!query.exec()) {
                qWarning() << "SQL Error (History Count):" << query.lastError().text() << "Query:" << query.lastQuery();
                throw query.lastError(); // Передаем ошибку дальше
            }
            chartValues << (query.next() ? query.value(0).toReal() : 0.0);

        }  else if (reportType == "Популярность книг (Топ-10)") {
            headers << "Название книги" << "Автор" << "Кол-во выдач";
            sql = QString( "SELECT b.title, b.author, COUNT(rh.book_id) as cnt FROM rental_history rh JOIN books b ON rh.book_id = b.id ")
                  + dateConditionHistory + (useFilterBinding ? filterClause : "") + // Добавляем фильтр, если нужно
                  " GROUP BY rh.book_id ORDER BY cnt DESC LIMIT 10";
            query.prepare(sql); // Готовим запрос
            if(useFilterBinding) query.bindValue(":filterValue", optionalFilter); // Биндим значение фильтра
            if (!query.exec()) throw query.lastError();
            while (query.next()) {
                dataRows << QString("%1,%2,%3").arg(query.value(0).toString(), query.value(1).toString(), query.value(2).toString());
            }

        } else if (reportType == "Популярность жанров") {
            isChartData = true;
            // Запрос не зависит от optionalFilter (т.к. группируем по жанрам)
            sql = QString( "SELECT b.genre, COUNT(rh.book_id) as cnt FROM rental_history rh JOIN books b ON rh.book_id = b.id ")
                  + dateConditionHistory + // Должен быть WHERE
                  " AND b.genre IS NOT NULL AND b.genre != '' GROUP BY b.genre ORDER BY cnt DESC"; // Используем AND
            if (!query.exec(sql)) throw query.lastError();
            while (query.next()) {
                chartLabels << query.value(0).toString();
                chartValues << query.value(1).toReal();
            }

        } else if (reportType == "Активные читатели (Топ-10)") {
            headers << "Логин читателя" << "Email" << "Кол-во взятых книг";
            sql = QString( "SELECT u.login, u.email, COUNT(rh.user_id) as cnt FROM rental_history rh JOIN users u ON rh.user_id = u.id ")
                  + dateConditionHistory + // Должен быть WHERE
                  (useFilterBinding ? filterClause : "") + // Добавляем фильтр, если нужно
                  " AND u.role = 'client' GROUP BY rh.user_id ORDER BY cnt DESC LIMIT 10"; // Используем AND
            query.prepare(sql); // Готовим запрос
            if(useFilterBinding) query.bindValue(":filterValue", optionalFilter); // Биндим значение фильтра
            if (!query.exec()) throw query.lastError();
            while (query.next()) {
                dataRows << QString("%1,%2,%3").arg(query.value(0).toString(), query.value(1).toString(), query.value(2).toString());
            }

        } else if (reportType == "Текущие просрочки") {
            headers << "ID аренды" << "Логин" << "Email" << "Название книги" << "Дата выдачи" << "Дата возврата" << "Дней просрочено";
            sql = QString( "SELECT r.id, u.login, u.email, b.title, r.start_date, r.end_date, "
                          "CAST(JULIANDAY('now', 'start of day') - JULIANDAY(r.end_date, 'start of day') AS INTEGER) as overdue_days "
                          "FROM rentals r JOIN users u ON r.user_id = u.id JOIN books b ON r.book_id = b.id "
                          "WHERE DATE(r.end_date) < DATE('now') " // Первое условие с WHERE
                          ) + (useFilterBinding ? filterClause : "") + // Добавляем фильтр с AND, если нужно
                  " ORDER BY overdue_days DESC, u.login";
            query.prepare(sql); // Готовим запрос
            if(useFilterBinding) query.bindValue(":filterValue", optionalFilter); // Биндим значение фильтра
            if (!query.exec()) throw query.lastError();
            while (query.next()) {
                dataRows << QString("%1,%2,%3,%4,%5,%6,%7").arg(query.value(0).toString(), query.value(1).toString(), query.value(2).toString(), query.value(3).toString(), query.value(4).toString(), query.value(5).toString(), query.value(6).toString());
            }

        } else if (reportType == "Состав фонда по жанрам") {
            isChartData = true;
            // Запрос не зависит от дат и фильтра
            sql = "SELECT genre, COUNT(*) as cnt FROM books WHERE genre IS NOT NULL AND genre != '' GROUP BY genre ORDER BY cnt DESC";
            if (!query.exec(sql)) throw query.lastError();
            while (query.next()) {
                chartLabels << query.value(0).toString();
                chartValues << query.value(1).toReal();
            }
        }
        else {
            qWarning() << "Unknown report type requested from database:" << reportType;
            return "error:Unknown report type requested.";
        }

    } catch (const QSqlError& error) {
        qCritical() << "SQL Error generating report [" << reportType << "]:" << error.text() << "Query:" << query.lastQuery();
        return "error:" + error.text(); // Возвращаем текст ошибки SQL
    } catch (const std::exception& e) {
        qCritical() << "Standard Exception generating report [" << reportType << "]:" << e.what();
        return "error:Internal server error during report generation.";
    }

    // =============================================
    // Формирование строки ответа для CommandHandler
    // =============================================
    if (isChartData) {
        // --- Формируем ответ для графика ---
        if (chartLabels.isEmpty() || chartValues.isEmpty()) {
            // Хотя запрос мог выполниться успешно, данных для графика нет
            return ""; // Возвращаем пустую строку
        }
        QStringList valueStrings;
        for(qreal v : chartValues) {
            valueStrings << QString::number(v, 'f', 2); // Форматируем число
        }
        return QString("chart:%1&%2&%3").arg(reportType, chartLabels.join(','), valueStrings.join(','));
    } else {
        // --- Формируем ответ для таблицы ---
        if (headers.isEmpty()) {
            // Ситуация не должна возникать, если логика выше корректна, но для безопасности:
            qWarning() << "Report [" << reportType << "] is marked as table data, but headers are empty.";
            return dataRows.isEmpty() ? "" : ("|" + dataRows.join('|')); // Отдаем данные без заголовков или ничего
        }

        if (dataRows.isEmpty()) {
            // Если есть заголовки, но нет строк данных
            return headers.join(',') + "|"; // Формат "Заголовки|" означает пустую таблицу
        } else {
            // Если есть и заголовки, и строки данных
            return headers.join(',') + "|" + dataRows.join('|');
        }
    }
}
