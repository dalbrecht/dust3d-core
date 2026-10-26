// SPDX-License-Identifier: MIT
#include <native/Engine/WorkerSessionScope.h>
#include <stdexcept>
using namespace dust3d::worker;
void require(bool value)
{
    if (!value)
        throw std::runtime_error("Session scope contract");
}
template <typename Action>
void rejects(Action action, SessionScopeErrorCode expected)
{
    bool rejected = false;
    try {
        action();
    } catch (const SessionScopeError& cause) {
        rejected = cause.code() == expected;
    }
    require(rejected);
}
int main()
{
    RequestIdentity request;
    request.document[0] = 1;
    request.request = 1;
    rejects([&] { SessionScope invalid({}, 1); }, SessionScopeErrorCode::invalidScope);
    rejects([&] { SessionScope invalid(request.document, 0); }, SessionScopeErrorCode::invalidScope);
    SessionScope scope(request.document, 7);
    scope.requireConnection(7);
    rejects([&] { scope.admit(request, 8); }, SessionScopeErrorCode::wrongConnection);
    auto wrong = request;
    wrong.document[0] = 2;
    rejects([&] { scope.admit(wrong, 7); }, SessionScopeErrorCode::wrongDocument);
    auto zero = request;
    zero.request = 0;
    rejects([&] { scope.admit(zero, 7); }, SessionScopeErrorCode::reusedRequest);
    scope.admit(request, 7);
    rejects([&] { scope.admit(request, 7); }, SessionScopeErrorCode::reusedRequest);
    request.request = 3;
    request.revision = 4;
    scope.admit(request, 7);
    request.request = 2;
    rejects([&] { scope.admit(request, 7); }, SessionScopeErrorCode::reusedRequest);
    request.request = 4;
    request.revision = 0;
    scope.admit(request, 7);
    SessionScope independent(request.document, 8);
    request.request = 1;
    independent.admit(request, 8);
    scope.close();
    scope.close();
    rejects([&] { scope.requireConnection(7); }, SessionScopeErrorCode::closed);
    rejects([&] { scope.admit(request, 7); }, SessionScopeErrorCode::closed);
    independent.requireConnection(8);
}
