#include <gtest/gtest.h>
#include "strike_commander/precomp.h"

TEST(SCFileRequesterTest, HasSaveExtension_MatchesAnyCase) {
    EXPECT_TRUE(SCFileRequester::hasSaveExtension("alex-001.sav"));
    EXPECT_TRUE(SCFileRequester::hasSaveExtension("A1.SAV"));
    EXPECT_TRUE(SCFileRequester::hasSaveExtension("mixed.Sav"));
}

TEST(SCFileRequesterTest, HasSaveExtension_RejectsOtherNames) {
    EXPECT_FALSE(SCFileRequester::hasSaveExtension("alex-001"));
    EXPECT_FALSE(SCFileRequester::hasSaveExtension(""));
    EXPECT_FALSE(SCFileRequester::hasSaveExtension(".sv"));
    EXPECT_FALSE(SCFileRequester::hasSaveExtension("config.ini"));
    EXPECT_FALSE(SCFileRequester::hasSaveExtension("savage"));
}
