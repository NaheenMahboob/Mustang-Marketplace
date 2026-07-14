CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -g -I.
LIBS_CLIENT := -lncurses -lcpp-httplib
LIBS_SERVER := -lsqlite3 -lcrypto -lcpp-httplib
LIBS_TEST := -lsqlite3 -lcrypto

# Shared domain/model implementation files reused by multiple build targets.
COMMON_DOMAIN_SRCS := Database.cpp User.cpp Listing.cpp Watchlist.cpp Admin.cpp Photo.cpp Recommendations.cpp Rating.cpp Offer.cpp Notification.cpp TsvUtil.cpp
# Acceptance-test source groupings used by the individual binary targets below.
ACCEPT_HELPERS := test/test_accept_helpers.cpp
AUTH_SESSION_TEST_SRC := test/test_accept_auth_session.cpp
LISTINGS_TEST_SRC := test/test_accept_listings.cpp
OFFERS_NOTIFICATIONS_TEST_SRC := test/test_accept_offers_notifications.cpp
WATCHLIST_RECOMMENDATIONS_TEST_SRC := test/test_accept_watchlist_recommendations.cpp
ADMIN_RATINGS_TEST_SRC := test/test_accept_admin_ratings.cpp

all: client server test_accept_binaries

.PHONY: all clean test_accept_binaries test_accept_all \
	test_accept_auth_session test_accept_listings test_accept_offers_notifications \
	test_accept_watchlist_recommendations test_accept_admin_ratings

test_accept_auth_session_bin: $(AUTH_SESSION_TEST_SRC) $(ACCEPT_HELPERS) Database.cpp User.cpp Listing.cpp Photo.cpp TsvUtil.cpp
	$(CXX) $(CXXFLAGS) -DTESTING $(AUTH_SESSION_TEST_SRC) $(ACCEPT_HELPERS) Database.cpp User.cpp Listing.cpp Photo.cpp TsvUtil.cpp -o test_accept_auth_session_bin $(LIBS_TEST)

test_accept_listings_bin: $(LISTINGS_TEST_SRC) $(ACCEPT_HELPERS) Database.cpp User.cpp Listing.cpp Photo.cpp TsvUtil.cpp
	$(CXX) $(CXXFLAGS) -DTESTING $(LISTINGS_TEST_SRC) $(ACCEPT_HELPERS) Database.cpp User.cpp Listing.cpp Photo.cpp TsvUtil.cpp -o test_accept_listings_bin $(LIBS_TEST)

test_accept_offers_notifications_bin: $(OFFERS_NOTIFICATIONS_TEST_SRC) $(ACCEPT_HELPERS) Database.cpp User.cpp Listing.cpp Watchlist.cpp Offer.cpp Notification.cpp Photo.cpp TsvUtil.cpp MarketplaceRules.h
	$(CXX) $(CXXFLAGS) -DTESTING $(OFFERS_NOTIFICATIONS_TEST_SRC) $(ACCEPT_HELPERS) Database.cpp User.cpp Listing.cpp Watchlist.cpp Offer.cpp Notification.cpp Photo.cpp TsvUtil.cpp -o test_accept_offers_notifications_bin $(LIBS_TEST)

test_accept_watchlist_recommendations_bin: $(WATCHLIST_RECOMMENDATIONS_TEST_SRC) $(ACCEPT_HELPERS) Database.cpp User.cpp Listing.cpp Watchlist.cpp Recommendations.cpp Photo.cpp TsvUtil.cpp
	$(CXX) $(CXXFLAGS) -DTESTING $(WATCHLIST_RECOMMENDATIONS_TEST_SRC) $(ACCEPT_HELPERS) Database.cpp User.cpp Listing.cpp Watchlist.cpp Recommendations.cpp Photo.cpp TsvUtil.cpp -o test_accept_watchlist_recommendations_bin $(LIBS_TEST)

test_accept_admin_ratings_bin: $(ADMIN_RATINGS_TEST_SRC) $(ACCEPT_HELPERS) Database.cpp User.cpp Admin.cpp Rating.cpp Listing.cpp Photo.cpp TsvUtil.cpp
	$(CXX) $(CXXFLAGS) -DTESTING $(ADMIN_RATINGS_TEST_SRC) $(ACCEPT_HELPERS) Database.cpp User.cpp Admin.cpp Rating.cpp Listing.cpp Photo.cpp TsvUtil.cpp -o test_accept_admin_ratings_bin $(LIBS_TEST)

test_accept_binaries: test_accept_auth_session_bin test_accept_listings_bin test_accept_offers_notifications_bin test_accept_watchlist_recommendations_bin test_accept_admin_ratings_bin

test_accept_auth_session: test_accept_auth_session_bin
	./test_accept_auth_session_bin

test_accept_listings: test_accept_listings_bin
	./test_accept_listings_bin

test_accept_offers_notifications: test_accept_offers_notifications_bin
	./test_accept_offers_notifications_bin

test_accept_watchlist_recommendations: test_accept_watchlist_recommendations_bin
	./test_accept_watchlist_recommendations_bin

test_accept_admin_ratings: test_accept_admin_ratings_bin
	./test_accept_admin_ratings_bin

test_accept_all: test_accept_auth_session test_accept_listings test_accept_offers_notifications test_accept_watchlist_recommendations test_accept_admin_ratings

client: main.cpp ApiClient.cpp
	$(CXX) $(CXXFLAGS) main.cpp ApiClient.cpp -o marketplace_client $(LIBS_CLIENT)

server: server_main.cpp ResponseBuilder.cpp Database.cpp User.cpp Listing.cpp Offer.cpp Watchlist.cpp Photo.cpp Recommendations.cpp Admin.cpp Rating.cpp Notification.cpp TsvUtil.cpp
	$(CXX) $(CXXFLAGS) server_main.cpp ResponseBuilder.cpp Database.cpp User.cpp Listing.cpp Offer.cpp Watchlist.cpp Photo.cpp Recommendations.cpp Admin.cpp Rating.cpp Notification.cpp TsvUtil.cpp -o marketplace_server $(LIBS_SERVER)

clean:
	rm -f marketplace_client marketplace_server \
		test_accept_auth_session_bin test_accept_listings_bin \
		test_accept_offers_notifications_bin test_accept_watchlist_recommendations_bin \
		test_accept_admin_ratings_bin
