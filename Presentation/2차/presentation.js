(() => {
  const deck = document.getElementById("deck");
  const slideOrder = [
    ".slide--cover",
    ".slide--overview-focus",
    ".slide--gameplay-video",
    ".slide--differentiation",
    ".slide--audience",
    ".slide--gameplay-loop",
    ".slide--local-ai",
    ".slide--world-art",
    ".character-grid",
    ".slide--multiplatform",
    ".slide--mobile-final",
    ".slide--final-polish",
    ".team-slide",
    ".slide--closing",
    ".slide--appendix-interface",
    ".slide--generative-ai",
  ];

  slideOrder.forEach((selector) => {
    const matched = document.querySelector(selector);
    const slide = matched?.classList.contains("slide") ? matched : matched?.closest(".slide");
    if (slide !== null && slide !== undefined) {
      deck.appendChild(slide);
    }
  });

  const slides = [...document.querySelectorAll(".slide")];
  slides.forEach((slide, index) => {
    slide.setAttribute("aria-label", `${index + 1}. ${slide.dataset.title}`);
  });
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
  const chatDemo = document.querySelector(".chat-demo");
  const chatViewport = document.querySelector(".chat-demo__viewport");

  let currentIndex = 0;
  let touchStartX = null;
  let chatDemoTimers = [];
  let cursorIdleTimer = null;

  const formatNumber = (value) => String(value).padStart(2, "0");

  const isPresentationFullscreen = () => {
    if (document.fullscreenElement !== null) {
      return true;
    }

    return (
      Math.abs(window.innerWidth - window.screen.width) <= 8 &&
      Math.abs(window.innerHeight - window.screen.height) <= 8
    );
  };

  const scheduleCursorHide = () => {
    window.clearTimeout(cursorIdleTimer);
    document.body.classList.remove("is-cursor-idle");

    if (!isPresentationFullscreen()) {
      return;
    }

    cursorIdleTimer = window.setTimeout(() => {
      document.body.classList.add("is-cursor-idle");
    }, 800);
  };

  const stopChatDemo = () => {
    chatDemoTimers.forEach((timer) => window.clearTimeout(timer));
    chatDemoTimers = [];
    chatDemo?.querySelectorAll("[data-chat-delay]").forEach((item) => item.classList.remove("is-visible"));
  };

  const scrollChatToBottom = (behavior = "smooth") => {
    window.requestAnimationFrame(() => {
      chatViewport?.scrollTo({ top: chatViewport.scrollHeight, behavior });
    });
  };

  const startChatDemo = () => {
    if (chatDemo === null || chatViewport === null) {
      return;
    }

    stopChatDemo();
    chatViewport.scrollTop = 0;

    chatDemo.querySelectorAll("[data-chat-delay]").forEach((item) => {
      const showDelay = Number(item.dataset.chatDelay);
      const hideDelay = Number(item.dataset.chatHide);

      chatDemoTimers.push(window.setTimeout(() => {
        item.classList.add("is-visible");
        scrollChatToBottom();
        chatDemoTimers.push(window.setTimeout(scrollChatToBottom, 560));
      }, showDelay));

      if (Number.isFinite(hideDelay)) {
        chatDemoTimers.push(window.setTimeout(() => item.classList.remove("is-visible"), hideDelay));
      }
    });

  };

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

    if (slides[currentIndex].contains(chatDemo)) {
      startChatDemo();
    } else {
      stopChatDemo();
    }
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
  window.addEventListener("resize", () => {
    scheduleCursorHide();
  });
  document.addEventListener("fullscreenchange", scheduleCursorHide);
  document.addEventListener("mousemove", scheduleCursorHide, { passive: true });

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
  scheduleCursorHide();
})();
