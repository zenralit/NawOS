#include <cmath>
#include <iostream>

namespace NawMath {
constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr double kEpsilon = 1e-12;

struct PolarNumber {
    double r;
    double phi;

    PolarNumber() : r(0.0), phi(0.0) {}
    PolarNumber(double radius, double angle) : r(radius), phi(angle) {}
};

struct NawNumber {
    double x;
    double y;

    NawNumber() : x(0.0), y(0.0) {}
    NawNumber(double real) : x(real), y(0.0) {}
    NawNumber(double real, double imag) : x(real), y(imag) {}
};

enum class QuadraticStatus {
    kOk,
    kLinear,
    kInfinite,
    kNone
};

struct QuadraticResult {
    QuadraticStatus status;
    NawNumber x1;
    NawNumber x2;

    QuadraticResult() : status(QuadraticStatus::kNone), x1(), x2() {}
};

inline bool IsZero(double value) {
    return std::fabs(value) <= kEpsilon;
}

inline bool IsZero(const NawNumber& value) {
    return IsZero(value.x) && IsZero(value.y);
}

inline PolarNumber ToPolar(const NawNumber& value) {
    return PolarNumber(std::sqrt(value.x * value.x + value.y * value.y), std::atan2(value.y, value.x));
}

inline NawNumber ToCartesian(const PolarNumber& value) {
    return NawNumber(value.r * std::cos(value.phi), value.r * std::sin(value.phi));
}

inline NawNumber operator+(const NawNumber& a, const NawNumber& b) {
    return NawNumber(a.x + b.x, a.y + b.y);
}

inline NawNumber operator-(const NawNumber& a, const NawNumber& b) {
    return NawNumber(a.x - b.x, a.y - b.y);
}

inline NawNumber operator*(const NawNumber& a, const NawNumber& b) {
    return NawNumber(a.x * b.x - a.y * b.y, a.x * b.y + a.y * b.x);
}

inline NawNumber operator/(const NawNumber& a, const NawNumber& b) {
    const double denom = b.x * b.x + b.y * b.y;
    return NawNumber((a.x * b.x + a.y * b.y) / denom, (a.y * b.x - a.x * b.y) / denom);
}

inline NawNumber operator*(const NawNumber& value, double factor) {
    return NawNumber(value.x * factor, value.y * factor);
}

inline NawNumber operator*(double factor, const NawNumber& value) {
    return value * factor;
}

inline NawNumber operator/(const NawNumber& value, double factor) {
    return NawNumber(value.x / factor, value.y / factor);
}

inline NawNumber operator-(const NawNumber& value) {
    return NawNumber(-value.x, -value.y);
}

inline double Abs(const NawNumber& value) {
    return std::sqrt(value.x * value.x + value.y * value.y);
}

inline double Arg(const NawNumber& value) {
    return std::atan2(value.y, value.x);
}

inline NawNumber Conjugate(const NawNumber& value) {
    return NawNumber(value.x, -value.y);
}

inline NawNumber Normalize(const NawNumber& value) {
    const double length = Abs(value);
    if (IsZero(length)) {
        return NawNumber();
    }
    return value / length;
}

inline NawNumber Pow(const NawNumber& value, double power) {
    const PolarNumber polar = ToPolar(value);
    return ToCartesian(PolarNumber(std::pow(polar.r, power), polar.phi * power));
}

inline NawNumber Sqrt(const NawNumber& value) {
    if (IsZero(value)) {
        return NawNumber();
    }

    const double magnitude = Abs(value);
    double real_term = (magnitude + value.x) * 0.5;
    double imag_term = (magnitude - value.x) * 0.5;

    if (real_term < 0.0) {
        real_term = 0.0;
    }

    if (imag_term < 0.0) {
        imag_term = 0.0;
    }

    double imag_part = std::sqrt(imag_term);
    if (value.y < 0.0) {
        imag_part = -imag_part;
    }

    return NawNumber(std::sqrt(real_term), imag_part);
}

inline NawNumber Sin(const NawNumber& value) {
    return NawNumber(std::sin(value.x) * std::cosh(value.y), std::cos(value.x) * std::sinh(value.y));
}

inline NawNumber Cos(const NawNumber& value) {
    return NawNumber(std::cos(value.x) * std::cosh(value.y), -std::sin(value.x) * std::sinh(value.y));
}

inline std::ostream& operator<<(std::ostream& out, const NawNumber& value) {
    out << "(" << value.x;
    if (value.y >= 0.0) {
        out << " + ";
    } else {
        out << " - ";
    }
    out << std::fabs(value.y) << "i)";
    return out;
}

inline std::istream& operator>>(std::istream& in, NawNumber& value) {
    return in >> value.x >> value.y;
}

inline QuadraticResult SolveQuadratic(const NawNumber& a, const NawNumber& b, const NawNumber& c) {
    QuadraticResult result;

    if (IsZero(a)) {
        if (IsZero(b)) {
            result.status = IsZero(c) ? QuadraticStatus::kInfinite : QuadraticStatus::kNone;
            return result;
        }

        result.status = QuadraticStatus::kLinear;
        result.x1 = -c / b;
        result.x2 = result.x1;
        return result;
    }

    const NawNumber discriminant = b * b - NawNumber(4.0) * a * c;
    const NawNumber sqrt_discriminant = Sqrt(discriminant);
    const NawNumber denominator = NawNumber(2.0) * a;

    result.status = QuadraticStatus::kOk;
    result.x1 = (-b + sqrt_discriminant) / denominator;
    result.x2 = (-b - sqrt_discriminant) / denominator;
    return result;
}
}  // namespace NawMath

using namespace NawMath;

static NawNumber ReadComplex(const char* label) {
    NawNumber value;
    std::cout << label << " = ";
    std::cin >> value;
    return value;
}

static void PrintComplexLine(const char* label, const NawNumber& value) {
    std::cout << label << " = " << value << "\n";
}

static void PrintScalarLine(const char* label, double value) {
    std::cout << label << " = " << value << "\n";
}

static void PrintQuadraticResult(const QuadraticResult& result) {
    if (result.status == QuadraticStatus::kLinear) {
        std::cout << "linear solution: x = " << result.x1 << "\n";
        return;
    }

    if (result.status == QuadraticStatus::kInfinite) {
        std::cout << "quadratic equation has infinitely many solutions\n";
        return;
    }

    if (result.status == QuadraticStatus::kNone) {
        std::cout << "quadratic equation has no solution\n";
        return;
    }

    PrintComplexLine("x1", result.x1);
    PrintComplexLine("x2", result.x2);
}

int main() {
    const NawNumber a = ReadComplex("a");
    const NawNumber b = ReadComplex("b");
    const NawNumber c = ReadComplex("c");

    PrintQuadraticResult(SolveQuadratic(a, b, c));
    std::cout << "\n";

    const NawNumber z = ReadComplex("z");

    PrintComplexLine("z", z);
    PrintScalarLine("abs(z)", Abs(z));
    PrintScalarLine("arg(z)", Arg(z));
    PrintComplexLine("conj(z)", Conjugate(z));
    PrintComplexLine("norm(z)", Normalize(z));
    PrintComplexLine("z^2", Pow(z, 2.0));
    PrintComplexLine("sqrt(z)", Sqrt(z));
    PrintComplexLine("sin(z)", Sin(z));
    PrintComplexLine("cos(z)", Cos(z));

    return 0;
}
