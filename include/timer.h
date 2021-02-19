//
// Created by liangyusen on 2018/8/3.
//

#ifndef RAFT_TIMER_H
#define RAFT_TIMER_H


namespace lraft {

    class Timer {
    private:
        std::shared_ptr<asio::io_service> _io_ptr;
        asio::deadline_timer _timer;
    public:
        Timer(std::shared_ptr<asio::io_service> io_ptr) : _io_ptr(io_ptr), _timer((*_io_ptr)) {}

        template<typename CALLBACK>
        void SetTimer(posix_time::milliseconds Delay, CALLBACK &&OnTimer, bool Repeat = false) {
            _timer.expires_from_now(Delay);
            _timer.async_wait([this, Delay, OnTimer, Repeat](const system::error_code &ec) {
                if (ec) {
                    // 代表计时器是被取消的
                } else {
                    OnTimer();
                    if (Repeat && !ec) {
                        this->SetTimer(Delay, OnTimer, Repeat);
                    }
                }

            });
        }

        void CancelTimer() {
            _timer.cancel();
        }

        void RestartTimer() {

        }
    };
}

#endif //RAFT_TIMER_H
