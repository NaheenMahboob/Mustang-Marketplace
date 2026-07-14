-- MustangMarketplace Database
-- SQLite

-- Users table
CREATE TABLE IF NOT EXISTS users (
    user_id       INTEGER PRIMARY KEY AUTOINCREMENT,
    email         TEXT NOT NULL UNIQUE,         -- must be @uwo.ca
    password_hash TEXT NOT NULL,                -- SHA-256 via OpenSSL
    created_at    DATETIME DEFAULT CURRENT_TIMESTAMP,
    is_banned     INTEGER DEFAULT 0             -- 0 = active, 1 = banned
);

-- Listings table
CREATE TABLE IF NOT EXISTS listings (
    listing_id  INTEGER PRIMARY KEY AUTOINCREMENT,
    seller_id   INTEGER NOT NULL,
    buyer_id    INTEGER,
    title       TEXT NOT NULL,
    description TEXT NOT NULL,
    price       REAL NOT NULL CHECK(price >= 0),
    category    TEXT,
    status      TEXT DEFAULT 'active',          -- 'active' | 'sold' | 'deleted'
    created_at  DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (seller_id) REFERENCES users(user_id),
    FOREIGN KEY (buyer_id)  REFERENCES users(user_id)
);

-- Watchlist table
CREATE TABLE IF NOT EXISTS watchlist (
    watchlist_id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id      INTEGER NOT NULL,
    listing_id   INTEGER NOT NULL,
    added_at     DATETIME DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(user_id, listing_id),
    FOREIGN KEY (user_id)    REFERENCES users(user_id),
    FOREIGN KEY (listing_id) REFERENCES listings(listing_id)
);

-- Photos table
CREATE TABLE IF NOT EXISTS photos (
    photo_id   INTEGER PRIMARY KEY AUTOINCREMENT,
    listing_id INTEGER NOT NULL,
    file_path  TEXT NOT NULL,                   -- relative or absolute path to image file
    FOREIGN KEY (listing_id) REFERENCES listings(listing_id)
);

-- Ratings table
CREATE TABLE IF NOT EXISTS ratings (
    rating_id  INTEGER PRIMARY KEY AUTOINCREMENT,
    listing_id INTEGER NOT NULL,
    buyer_id   INTEGER NOT NULL,
    seller_id  INTEGER NOT NULL,
    rating     INTEGER NOT NULL CHECK(rating >= 1 AND rating <= 5),
    comment    TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(listing_id),
    FOREIGN KEY (listing_id) REFERENCES listings(listing_id),
    FOREIGN KEY (buyer_id)  REFERENCES users(user_id),
    FOREIGN KEY (seller_id) REFERENCES users(user_id)
);

-- Offers table
CREATE TABLE IF NOT EXISTS offers (
    offer_id    INTEGER PRIMARY KEY AUTOINCREMENT,
    listing_id  INTEGER NOT NULL,
    buyer_id    INTEGER NOT NULL,
    seller_id   INTEGER NOT NULL,
    offer_price REAL NOT NULL CHECK(offer_price > 0),
    status      TEXT NOT NULL DEFAULT 'Pending', -- Pending | Closed | Rejected
    created_at  DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (listing_id) REFERENCES listings(listing_id),
    FOREIGN KEY (buyer_id)   REFERENCES users(user_id),
    FOREIGN KEY (seller_id)  REFERENCES users(user_id)
);

-- Notifications table
CREATE TABLE IF NOT EXISTS notifications (
    notification_id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id         INTEGER NOT NULL,
    message         TEXT NOT NULL,
    created_at      DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES users(user_id)
);
