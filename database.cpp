#include "database.h"

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
                          "role TEXT NOT NULL)");

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

    if (!success) {
        qDebug() << "Error creating/updating tables:" << query.lastError();
        // ★ Попытка добавить столбец, если таблица уже существует
        if (!query.exec("ALTER TABLE books ADD COLUMN annotation TEXT DEFAULT ''")) {
            qDebug() << "Failed to add annotation column:" << query.lastError();
                // Это не фатальная ошибка, если таблица уже была с колонкой
        } else {
            qDebug() << "Successfully added 'annotation' column to 'books' table.";
        }
        success = true; // Продолжаем работу, даже если alter table не удался (мог уже существовать)
    }
    return success;
}

// Common methods
bool database::authenticateUser(const QString& login, const QString& password) {
    QSqlQuery query;
    query.prepare("SELECT password FROM users WHERE login = :login");
    query.bindValue(":login", login);

    if (!query.exec()) {
        qDebug() << "Auth error:" << query.lastError().text();
        return false;
    }

    if (query.next()) {
        QString dbPassword = query.value(0).toString();
        return (dbPassword == password);
    }
    return false;
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

    QSqlQuery query;
    query.prepare("DELETE FROM rentals WHERE user_id = :user_id AND book_id = :book_id");
    query.bindValue(":user_id", user_id);
    query.bindValue(":book_id", book_id);

    if (!query.exec()) {
        qDebug() << "Error unassigning book:" << query.lastError().text();
        return false;
    }

    // Mark book as available
    query.prepare("UPDATE books SET is_available = 1 WHERE id = :book_id");
    query.bindValue(":book_id", book_id);
    return query.exec();
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
    QSqlQuery query("SELECT id, login FROM users WHERE role = 'client' ORDER BY login ASC");

    if (!query.exec()) {
        qDebug() << "Error getting client users:" << query.lastError().text();
        return users; // Возвращаем пустой список в случае ошибки
    }

    while (query.next()) {
        users << QString("%1,%2")
        .arg(query.value(0).toString()) // id
            .arg(query.value(1).toString()); // login
    }
    return users;
}
