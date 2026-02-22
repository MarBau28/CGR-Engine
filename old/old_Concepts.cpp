#include "old_Concepts.h"

// int main() {

    // std::array<int, 5> arr = { 1, 2, 3, 4, 5 };
    // for (const int& elem : arr) {
    //     std::cout << elem << " ";  // Output: 1 2 3 4 5
    // }

    //const size_t size {10};
    /*double* temperatures { new double[size] {10.0, 20.0, 30.0, 40.0, 50.0, 60.0, 70.0, 80.0, 90.0, 100.0} };

    for (double& temp : temperatures) {
        std::cout << "temperature: " << temp << std::endl;
    }*/
    //std::string test { "sfsd" };
    //int test2 { 1 + 2 + 3 };
    //short int test3 { 1 };
    //char test4 { 2 };
    //auto test5 { test3 + test4 };
    //test2 = 55.9;
    /*char message[]{ 'h', 'e', 'l', 'l', 'o' };

    std::cout << message << std::endl;
    std::cout << std::size(message) << std::endl;*/

    /*std::string myValue = "42";

    std::string result = "My value is " + myValue + ".";

    std::cout << result << std::endl;*/

    /*int test{ 10 };
    const int& refTest{ test };
    test = 20;

    std::cout << test << std::endl;
    std::cout << refTest << std::endl;*/

    //int a{ 1 };
    //int b{ 2 };
    //int c{ 3 };
    //int d{ 4 };
    //std::string s1 { "TEST1" };
    //std::string s2 { "TEST2" };

    //int func = [&a, &b](int one, int two) -> int {
    //    return one + two + a + b;
    //}(c, d);

    ////std::cout << func << std::endl;

    ////std::cout << add(a, b) << std::endl;

    //Cylinder cyObject(10, 20);

    //std::cout << cyObject.get_baseRadius() << std::endl;

    //asio::io_context io_context;
    //asio::steady_timer timer(io_context, std::chrono::seconds(3));

    //std::cout << "Starting asynchronous timer for 3 seconds..." << std::endl;

    //// Initiate asynchronous wait, providing a callback function
    //timer.async_wait(timer_callback);

    //std::cout << "Doing other work while waiting for the timer..." << std::endl;
    //// The io_context.run() call below is crucial to start the event loop
    //io_context.run(); // Run the Asio event loop (blocks until io_context is stopped)

    //std::cout << "io_context::run() finished." << std::endl; // Will reach here after timer expires

    // Initialize COM for WinRT (required for C++/WinRT)
    //init_apartment();

    // Run the asynchronous main coroutine and block until it completes
    //run_async_main().get(); // .get() on IAsyncAction blocks until completion

    //std::cout << "Main: Example finished." << std::endl;

//     return 0;
// }

// template <typename T>
// concept MyConcept = requires (T a, T b) {
//     a+=1;
//     ++a;
//     a++;
//     a*b;
//     b<=3;
//     requires sizeof(b) <= 3;
//     { a + b } noexcept -> std::convertible_to<int>;
// };
//
// template <typename T>
// concept MyConcept2 = requires (T a, T b) {
//     a += 1;
//     ++a;
//     a++;
//     a* b;
//     b <= 3;
//         requires sizeof(b) <= 3;
//     { a + b } noexcept -> std::convertible_to<int>;
// };
//
// template <typename T>
// requires MyConcept<T> || MyConcept<T>
// T add(T a, T b) {
//     return a + b;
// }
//
// static void timer_callback(const asio::error_code& error) {
//     if (!error) {
//         std::cout << "Asynchronous timer expired!" << std::endl;
//     }
//     else {
//         std::cerr << "Timer error: " << error.message() << std::endl;
//     }
// }
//
// static IAsyncOperation<int> async_calculate_value_coroutine() {
//     std::cout << "Coroutine: Starting asynchronous calculation..." << std::endl;
//     co_await ThreadPool::RunAsync([](IAsyncAction) {
//         std::cout << "Coroutine: Doing work in thread pool..." << std::endl;
//         std::this_thread::sleep_for(seconds(1)); // Simulate work
//         });
//     std::cout << "Coroutine: Calculation complete." << std::endl;
//     co_return 42; // Return the result
// }
//
// // Helper coroutine to wrap main (since main can't directly be a coroutine in standard C++)
// static IAsyncAction run_async_main() {
//     std::cout << "Main: Starting coroutine example..." << std::endl;
//
//     IAsyncOperation<int> async_op =  async_calculate_value_coroutine();
//
//     std::cout << "Main: Doing other work while waiting..." << std::endl;
//     co_await ThreadPool::RunAsync([](IAsyncAction) {
//         std::this_thread::sleep_for(seconds(2)); // Simulate main thread work
//         });
//
//     std::cout << "Main: Awaiting coroutine result..." << std::endl;
//     int result = co_await async_op; // Await the result of the coroutine
//     std::cout << "Main: Coroutine result: " << result << std::endl;
// }