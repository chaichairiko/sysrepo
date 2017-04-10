#include <iostream>
#include <memory>
#include <cassert>
#include <cstring>
#include <unistd.h>

#include "Session.h"

#define MAX_LEN 100

using namespace std;

const string module_name = "swig-test-cpp-changes";
const int LOW_BOUND = 10;
const int HIGH_BOUND = 20;

std::string get_test_name(int i)
{
    return "test-cpp-" + to_string(i);
}

std::string get_xpath(const std::string &test_name, const std::string &node_name)
{
    return "/" + module_name + ":cpp-changes/test-get[name='" + test_name + "']/" + node_name;
}

class NopCallback: public Callback {
public:
    int module_change(S_Session sess, const char *module_name, sr_notif_event_t event, void *private_ctx)
        {
            return SR_ERR_OK;
        }
};

void init_test(S_Session sess)
{
    S_Subscribe subs(new Subscribe(sess));
    S_Callback cb(new NopCallback());

    subs->module_change_subscribe(module_name.c_str(), cb, NULL, 0, SR_SUBSCR_DEFAULT | SR_SUBSCR_APPLY_ONLY);

    for (int i = LOW_BOUND; i < HIGH_BOUND; i++) {
        const auto xpath = get_xpath(get_test_name(i), "number");
        S_Val vset(new Val((int32_t)i, SR_INT32_T));
        sess->set_item(xpath.c_str(), vset);
    }

    sess->commit();
    sess->copy_config(module_name.c_str(), SR_DS_RUNNING, SR_DS_STARTUP);
    subs->unsubscribe();
}

void
clean_test(S_Session sess)
{
    string module_name_tmp(module_name);
    const string xpath = "/" + module_name_tmp + ":*";

    sess->delete_item(xpath.c_str());
    sess->commit();
    // sess.copy_config(module_name, sr_datastore_t.SR_DS_RUNNING, sr_datastore_t.SR_DS_STARTUP);
}

class DeleteCb: public Callback {
public:
    int module_change(S_Session sess, const char *module_name, sr_notif_event_t event, void *private_ctx)
        {
            char change_path[MAX_LEN];

            snprintf(change_path, MAX_LEN, "/%s:*", module_name);
            auto it = sess->get_changes_iter(&change_path[0]);

            while (auto change = sess->get_change_next(it)) {
                assert(SR_OP_DELETED == change->oper());
            }
            return SR_ERR_OK;
        }
};

void
test_module_change_delete(S_Session sess)
{
    S_Subscribe subs(new Subscribe(sess));
    S_Callback cb(new DeleteCb());

    init_test(sess);
    subs->module_change_subscribe(module_name.c_str(), cb, NULL, 0, SR_SUBSCR_DEFAULT | SR_SUBSCR_APPLY_ONLY);

    const auto xpath = get_xpath(get_test_name(LOW_BOUND), "number");
    sess->delete_item(xpath.c_str());
    sess->commit();

    subs->unsubscribe();
}

class ModifyCb: public Callback {
public:
    int module_change(S_Session sess, const char *module_name, sr_notif_event_t event, void *private_ctx)
        {
            char change_path[MAX_LEN];

            snprintf(change_path, MAX_LEN, "/%s:*", module_name);
            auto it = sess->get_changes_iter(&change_path[0]);

            while (auto change = sess->get_change_next(it)) {
                assert(SR_OP_MODIFIED == change->oper());
            }
            return SR_ERR_OK;
        }
};

void
test_module_change_modify(S_Session sess)
{
    S_Subscribe subs(new Subscribe(sess));
    S_Callback cb(new ModifyCb());

    init_test(sess);

    subs->module_change_subscribe(module_name.c_str(), cb, NULL, 0, SR_SUBSCR_DEFAULT | SR_SUBSCR_APPLY_ONLY);

    const auto xpath = get_xpath(get_test_name(LOW_BOUND), "number");
    S_Val vset(new Val((int32_t)42, SR_INT32_T));
    sess->set_item(xpath.c_str(), vset);
    sess->commit();
    subs->unsubscribe();
}

class CreateCb: public Callback {
public:
    int module_change(S_Session sess, const char *module_name, sr_notif_event_t event, void *private_ctx)
        {
            char change_path[MAX_LEN];

            snprintf(change_path, MAX_LEN, "/%s:*", module_name);
            auto it = sess->get_changes_iter(&change_path[0]);

            while (auto change = sess->get_change_next(it)) {
                assert(SR_OP_CREATED == change->oper());
            }
            return SR_ERR_OK;
        }
};

void
test_module_change_create(S_Session sess)
{
    S_Subscribe subs(new Subscribe(sess));
    S_Callback cb(new CreateCb());

    init_test(sess);

    subs->module_change_subscribe(module_name.c_str(), cb, NULL, 0, SR_SUBSCR_DEFAULT | SR_SUBSCR_APPLY_ONLY);

    const auto xpath = get_xpath(get_test_name(HIGH_BOUND), "number");
    S_Val vset(new Val((int32_t)42, SR_INT32_T));
    sess->set_item(xpath.c_str(), vset);
    sess->commit();

    subs->unsubscribe();
}

int
main(int argc, char **argv)
{
    int n_try = 3;
    while(n_try-- > 0) {
        try {
            S_Connection conn(new Connection("test changes"));
            S_Session sess(new Session(conn, SR_DS_RUNNING));

            clean_test(sess);
            test_module_change_delete(sess);
            clean_test(sess);
            test_module_change_modify(sess);
            test_module_change_create(sess);
            return 0;
        } catch (const std::exception& e) {
            cout << e.what() << endl;
            usleep(1000);
        }
    }

    assert(false);
}
