(() => {
  const slides = [...document.querySelectorAll(".slide")];
  const menu = document.getElementById("slideMenu");
  const menuList = document.getElementById("menuList");
  const menuToggle = document.getElementById("menuToggle");
  const menuClose = document.getElementById("menuClose");
  const menuScrim = document.getElementById("menuScrim");
  const prevButton = document.getElementById("prevButton");
  const nextButton = document.getElementById("nextButton");
  const currentNumber = document.getElementById("currentNumber");
  const totalNumber = document.getElementById("totalNumber");
  const progressBar = document.getElementById("progressBar");

  let currentIndex = 0;
  let touchStartX = null;

  const formatNumber = (value) => String(value).padStart(2, "0");

  const setMenuOpen = (open) => {
    menu.classList.toggle("is-open", open);
    menuScrim.classList.toggle("is-open", open);
    menu.setAttribute("aria-hidden", String(!open));
    menuToggle.setAttribute("aria-expanded", String(open));

    if (open) {
      menu.querySelector(".menu-item.is-active")?.focus();
    } else {
      menuToggle.focus({ preventScroll: true });
    }
  };

  const updateSlides = () => {
    slides.forEach((slide, index) => {
      slide.classList.toggle("is-active", index === currentIndex);
      slide.classList.toggle("is-before", index < currentIndex);
      slide.setAttribute("aria-hidden", String(index !== currentIndex));
    });

    [...menuList.children].forEach((item, index) => {
      item.classList.toggle("is-active", index === currentIndex);
      item.setAttribute("aria-current", index === currentIndex ? "page" : "false");
    });

    currentNumber.textContent = formatNumber(currentIndex + 1);
    totalNumber.textContent = formatNumber(slides.length);
    progressBar.style.width = `${((currentIndex + 1) / slides.length) * 100}%`;
    prevButton.disabled = currentIndex === 0;
    nextButton.disabled = currentIndex === slides.length - 1;
    document.title = `${slides[currentIndex].dataset.title} | TRAIP AI : RE`;
  };

  const goToSlide = (index) => {
    const nextIndex = Math.max(0, Math.min(index, slides.length - 1));
    if (nextIndex === currentIndex) {
      return;
    }

    currentIndex = nextIndex;
    updateSlides();
  };

  slides.forEach((slide, index) => {
    const button = document.createElement("button");
    button.type = "button";
    button.className = "menu-item";
    button.innerHTML = `<span>${formatNumber(index + 1)}</span><strong>${slide.dataset.title}</strong>`;
    button.addEventListener("click", () => {
      goToSlide(index);
      setMenuOpen(false);
    });
    menuList.appendChild(button);
  });

  prevButton.addEventListener("click", () => goToSlide(currentIndex - 1));
  nextButton.addEventListener("click", () => goToSlide(currentIndex + 1));
  menuToggle.addEventListener("click", () => setMenuOpen(!menu.classList.contains("is-open")));
  menuClose.addEventListener("click", () => setMenuOpen(false));
  menuScrim.addEventListener("click", () => setMenuOpen(false));

  document.addEventListener("keydown", (event) => {
    const isMenuOpen = menu.classList.contains("is-open");

    if (event.key === "Escape" && isMenuOpen) {
      event.preventDefault();
      setMenuOpen(false);
      return;
    }

    if (event.key.toLowerCase() === "m") {
      event.preventDefault();
      setMenuOpen(!isMenuOpen);
      return;
    }

    if (isMenuOpen) {
      return;
    }

    if (["ArrowRight", "ArrowDown", "PageDown", " "].includes(event.key)) {
      event.preventDefault();
      goToSlide(currentIndex + 1);
    } else if (["ArrowLeft", "ArrowUp", "PageUp", "Backspace"].includes(event.key)) {
      event.preventDefault();
      goToSlide(currentIndex - 1);
    } else if (event.key === "Home") {
      event.preventDefault();
      goToSlide(0);
    } else if (event.key === "End") {
      event.preventDefault();
      goToSlide(slides.length - 1);
    }
  });

  document.addEventListener("touchstart", (event) => {
    touchStartX = event.changedTouches[0]?.clientX ?? null;
  }, { passive: true });

  document.addEventListener("touchend", (event) => {
    if (touchStartX === null || menu.classList.contains("is-open")) {
      return;
    }

    const touchEndX = event.changedTouches[0]?.clientX ?? touchStartX;
    const delta = touchEndX - touchStartX;
    touchStartX = null;

    if (Math.abs(delta) < 50) {
      return;
    }

    goToSlide(currentIndex + (delta < 0 ? 1 : -1));
  }, { passive: true });

  updateSlides();
})();
