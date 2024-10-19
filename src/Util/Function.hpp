#pragma once

namespace Util
{
    template<class...Fs>
    struct overloaded : Fs... {
    using Fs::operator()...;
    };

    template<class...Fs>
    overloaded(Fs...) -> overloaded<Fs...>;
}