import { useState, useEffect } from 'react'
import { Link, useLocation } from 'react-router-dom'

function Header() {
  const [isMenuOpen, setIsMenuOpen] = useState(false)
  const [isScrolled, setIsScrolled] = useState(false)
  const location = useLocation()

  useEffect(() => {
    const handleScroll = () => {
      setIsScrolled(window.scrollY > 20)
    }
    window.addEventListener('scroll', handleScroll)
    return () => window.removeEventListener('scroll', handleScroll)
  }, [])

  useEffect(() => {
    setIsMenuOpen(false)
  }, [location])

  const toggleMenu = () => {
    setIsMenuOpen(!isMenuOpen)
  }

  const isLoggedIn = localStorage.getItem('user') !== null
  const isAuthPage = ['/login', '/signup', '/forgot-password'].includes(location.pathname)

  const handleLogout = () => {
    localStorage.removeItem('user')
    localStorage.removeItem('token')
    window.location.href = '/login'
  }

  return (
    <header className={`fixed top-0 left-0 right-0 z-[1000] transition-all duration-300 ${
      isScrolled ? 'shadow-2xl' : 'shadow-lg'
    } ${isAuthPage ? 'h-20' : 'h-[70px]'}`}>
      {/* Background */}
      <div className="absolute inset-0 bg-gradient-to-br from-purple-600 to-purple-800 border-b border-black/10">
        {/* Shimmer effect */}
        <div className="absolute inset-0 overflow-hidden">
          <div className="absolute top-0 -left-full w-full h-full bg-gradient-to-r from-transparent via-white/30 to-transparent animate-[shimmer_5s_ease-in-out_infinite]"></div>
        </div>

        {/* Floating shapes for auth pages */}
        {isAuthPage && (
          <div className="absolute inset-0 overflow-visible">
            {[
              { size: 80, top: '20%', left: '10%', delay: 0 },
              { size: 120, top: '60%', left: '80%', delay: 1 },
              { size: 60, top: '80%', left: '20%', delay: 2 },
              { size: 100, top: '10%', left: '70%', delay: 3 },
              { size: 40, top: '50%', left: '50%', delay: 4 }
            ].map((shape, i) => (
              <div
                key={i}
                className="absolute rounded-full bg-white/10 animate-[float_6s_ease-in-out_infinite]"
                style={{
                  width: shape.size,
                  height: shape.size,
                  top: shape.top,
                  left: shape.left,
                  animationDelay: `${shape.delay}s`
                }}
              ></div>
            ))}
          </div>
        )}
      </div>

      {/* Container */}
      <div className="relative max-w-7xl mx-auto px-4 md:px-8 h-full flex items-center justify-between">
        {/* Logo */}
        <Link to="/" className="flex items-center gap-3 text-white no-underline transition-transform duration-300 hover:-translate-y-0.5 z-10">
          <div className="w-10 h-10 md:w-12 md:h-12 bg-gradient-to-br from-purple-600 to-purple-800 rounded-xl flex items-center justify-center border-2 border-white/20 shadow-lg">
            <svg viewBox="0 0 24 24" fill="none" className="w-6 h-6 text-white">
              <path d="M12 2L2 7L12 12L22 7L12 2Z" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"/>
              <path d="M2 17L12 22L22 17" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"/>
              <path d="M2 12L12 17L22 12" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"/>
            </svg>
          </div>
          <div className="flex flex-col">
            <span className="text-xl md:text-2xl font-extrabold leading-tight">Lab Expert</span>
            {isAuthPage && (
              <span className="text-xs md:text-sm opacity-90 font-medium tracking-wide hidden sm:block">
                Virtual Laboratory
              </span>
            )}
          </div>
        </Link>

        {/* Desktop Navigation - Hide on auth pages */}
        {!isAuthPage && (
          <nav className="hidden lg:flex flex-1 justify-center">
            <ul className="flex items-center gap-2 m-0 p-0 list-none">
              {[
                { path: '/home', label: 'Home' },
                { path: '/about', label: 'About' },
                { path: '/services', label: 'Services' },
                { path: '/contact', label: 'Contact' }
              ].map((item) => (
                <li key={item.path}>
                  <Link
                    to={item.path}
                    className={`relative px-5 py-3 text-white font-semibold rounded-xl no-underline transition-all duration-300 hover:-translate-y-0.5 ${
                      location.pathname === item.path
                        ? 'bg-white/20 backdrop-blur-sm'
                        : 'hover:bg-white/10'
                    }`}
                  >
                    {item.label}
                  </Link>
                </li>
              ))}
            </ul>
          </nav>
        )}

        {/* Auth Buttons - Hide on auth pages */}
        {!isAuthPage && (
          <div className="hidden lg:flex items-center gap-4">
            {!isLoggedIn ? (
              <>
                <Link
                  to="/login"
                  className="px-5 py-3 bg-white/20 text-white font-semibold rounded-xl border-2 border-white no-underline transition-all duration-300 hover:bg-purple-700 hover:-translate-y-0.5 hover:shadow-xl"
                >
                  Login
                </Link>
                <Link
                  to="/signup"
                  className="px-5 py-3 bg-gradient-to-br from-purple-600 to-purple-800 text-white font-semibold rounded-xl border-2 border-transparent no-underline transition-all duration-300 hover:-translate-y-0.5 hover:shadow-xl"
                >
                  Sign Up
                </Link>
              </>
            ) : (
              <button
                onClick={handleLogout}
                className="px-5 py-3 bg-white/20 text-white font-semibold rounded-xl border-2 border-white cursor-pointer transition-all duration-300 hover:bg-purple-700 hover:-translate-y-0.5 hover:shadow-xl"
              >
                Logout
              </button>
            )}
          </div>
        )}

        {/* Mobile Menu Button - Hide on auth pages */}
        {!isAuthPage && (
          <button
            className="lg:hidden flex flex-col justify-between w-8 h-6 bg-transparent border-0 cursor-pointer p-0 z-10"
            onClick={toggleMenu}
            aria-label="Toggle menu"
          >
            <span className={`w-full h-0.5 bg-white rounded transition-all duration-300 origin-left ${
              isMenuOpen ? 'rotate-45 translate-x-1 translate-y-1' : ''
            }`}></span>
            <span className={`w-full h-0.5 bg-white rounded transition-all duration-300 ${
              isMenuOpen ? 'opacity-0' : ''
            }`}></span>
            <span className={`w-full h-0.5 bg-white rounded transition-all duration-300 origin-left ${
              isMenuOpen ? '-rotate-45 translate-x-1 -translate-y-1' : ''
            }`}></span>
          </button>
        )}
      </div>

      {/* Mobile Navigation - Hide on auth pages */}
      {!isAuthPage && (
        <>
          <nav className={`lg:hidden fixed top-[70px] left-0 w-full h-[calc(100vh-70px)] bg-white/95 backdrop-blur-xl transition-transform duration-300 z-[1100] overflow-y-auto ${
            isMenuOpen ? 'translate-x-0' : '-translate-x-full'
          }`}>
            <ul className="flex flex-col gap-2 p-8 m-0 list-none min-h-full">
              {[
                { path: '/', label: 'Home' },
                { path: '/about', label: 'About' },
                { path: '/services', label: 'Services' },
                { path: '/contact', label: 'Contact' }
              ].map((item) => (
                <li key={item.path}>
                  <Link
                    to={item.path}
                    className={`flex items-center gap-4 px-6 py-4 font-semibold rounded-xl no-underline transition-all duration-300 ${
                      location.pathname === item.path
                        ? 'bg-gradient-to-br from-purple-600 to-purple-800 text-white translate-x-2'
                        : 'text-slate-800 hover:bg-gradient-to-br hover:from-purple-600 hover:to-purple-800 hover:text-white hover:translate-x-2'
                    }`}
                  >
                    {item.label}
                  </Link>
                </li>
              ))}

              <li className="h-px bg-gradient-to-r from-transparent via-purple-600 to-transparent my-4"></li>

              {!isLoggedIn ? (
                <>
                  <li>
                    <Link
                      to="/login"
                      className="flex items-center gap-4 px-6 py-4 text-slate-800 font-semibold rounded-xl no-underline transition-all duration-300 hover:bg-gradient-to-br hover:from-purple-600 hover:to-purple-800 hover:text-white hover:translate-x-2"
                    >
                      Login
                    </Link>
                  </li>
                  <li>
                    <Link
                      to="/signup"
                      className="flex items-center gap-4 px-6 py-4 text-slate-800 font-semibold rounded-xl no-underline transition-all duration-300 hover:bg-gradient-to-br hover:from-purple-600 hover:to-purple-800 hover:text-white hover:translate-x-2"
                    >
                      Sign Up
                    </Link>
                  </li>
                </>
              ) : (
                <li>
                  <button
                    onClick={handleLogout}
                    className="w-full flex items-center gap-4 px-6 py-4 text-slate-800 font-semibold rounded-xl bg-transparent border-0 cursor-pointer text-left transition-all duration-300 hover:bg-gradient-to-br hover:from-purple-600 hover:to-purple-800 hover:text-white hover:translate-x-2"
                  >
                    Logout
                  </button>
                </li>
              )}
            </ul>
          </nav>

          {/* Mobile Overlay */}
          {isMenuOpen && (
            <div
              className="lg:hidden fixed top-[70px] left-0 w-full h-[calc(100vh-70px)] bg-black/50 z-[1099]"
              onClick={() => setIsMenuOpen(false)}
            ></div>
          )}
        </>
      )}

      <style jsx>{`
        @keyframes shimmer {
          0% { transform: translateX(0); }
          100% { transform: translateX(200%); }
        }
        @keyframes float {
          0%, 100% {
            transform: translateY(0px) rotate(0deg);
            opacity: 0.7;
          }
          50% {
            transform: translateY(-10px) rotate(180deg);
            opacity: 1;
          }
        }
      `}</style>
    </header>
  )
}

export default Header